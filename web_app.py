#!/usr/bin/env python3
"""
web_app.py - runs the Smart Revision Platform as a local website.

    python web_app.py        (Windows)
    python3 web_app.py       (macOS / Linux)

Who does what:
  C++  (smart_revision --api)  accounts, logins, OTP checks, saving, the question
                               bank and question set (practice), answer checking,
                               statistics, heap, BFS.
  Python (this file)           serves the page, and does the two jobs that need
                               the internet: talking to the AI (question_ai.py)
                               and sending the OTP email (mailer.py).
  Browser (web/index.html)     draws the screens. It never sees an answer key,
                               an OTP, or the API key.

Only the Python standard library is used. The server listens on 127.0.0.1,
so other computers cannot connect.
"""

import json
import os
import platform
import secrets
import subprocess
import sys
import threading
import time
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

import mailer
import question_ai
import settings
from ai_client import AIError

HOST = "127.0.0.1"
FIRST_PORT = 8000
PROJECT_DIR = os.path.dirname(os.path.abspath(__file__))
PAGE_PATH = os.path.join(PROJECT_DIR, "web", "index.html")
EXE_NAME = "smart_revision.exe" if platform.system() == "Windows" else "smart_revision"
EXE_PATH = os.path.join(PROJECT_DIR, EXE_NAME)
DATA_DIR = os.environ.get("SRP_DATA_DIR", "data")   # test_web.py uses a separate folder
DATASET_DIR = os.path.join(PROJECT_DIR, "dataset")
# The question set used for Practice (written by import_dataset.py from the sheet in dataset/).
DATASET_FILE = os.environ.get("SRP_DATASET", os.path.join("dataset", "questions_dataset.txt"))

# Commands the browser may send, and how many arguments each one takes.
# Internal C++ commands (otp_start, prepare, practice_start, import_question, start_ids,
# append_ids, current_info, answer_graded, mistake_info, save_note) are NOT in this list,
# so a browser can never call them directly.
ALLOWED_COMMANDS = {
    "hello": 0, "setup": 3, "register": 3, "login": 3, "logout": 1,
    "forgot_start": 2, "reset_password": 4,
    "syllabus": 1, "generate": 7, "next": 1, "answer": 2, "review": 2,
    "report": 1, "plan": 1, "mistakes": 1, "explain_mistake": 2,
    "admin_overview": 1, "admin_student": 2,
}
MAX_BODY_BYTES = 40_000
MAX_ARG_LENGTH = 500
MAX_ANSWER_LENGTH = 8000   # a code answer can be long


def needs_build():
    if not os.path.exists(EXE_PATH):
        return True
    exe_time = os.path.getmtime(EXE_PATH)
    for name in os.listdir(PROJECT_DIR):
        if name.endswith((".cpp", ".h")):
            if os.path.getmtime(os.path.join(PROJECT_DIR, name)) > exe_time:
                return True
    return False


def build():
    print("Building the C++ back end...")
    cmd = ["g++", "-std=c++11", "-o", EXE_NAME, "main.cpp"]
    try:
        result = subprocess.run(cmd, cwd=PROJECT_DIR)
    except FileNotFoundError:
        print("g++ was not found. Install a C++ compiler (e.g. MinGW-w64) and add it to PATH.")
        return False
    if result.returncode != 0:
        print("Build failed - see the compiler errors above.")
        return False
    print("Build finished.")
    return True


def escape_field(value):
    """Same rule as Storage::escape in C++, so code with tabs and new lines survives."""
    return (str(value).replace("\\", "\\\\").replace("\t", "\\t")
            .replace("\n", "\\n").replace("\r", "\\r"))


class Backend:
    """Keeps one C++ process running and talks to it one line at a time."""

    def __init__(self):
        self.process = None
        self.lock = threading.Lock()

    def _start(self):
        self.process = subprocess.Popen(
            [EXE_PATH, "--api", DATA_DIR, DATASET_FILE], cwd=PROJECT_DIR,
            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            text=True, encoding="utf-8", bufsize=1,
        )

    def call(self, command, args):
        line = "\t".join([command] + [escape_field(a) for a in args]) + "\n"
        with self.lock:
            if self.process is None or self.process.poll() is not None:
                self._start()
            try:
                self.process.stdin.write(line)
                self.process.stdin.flush()
                reply = self.process.stdout.readline()
            except (BrokenPipeError, OSError):
                reply = ""
            if not reply:
                self.process = None
                return {"ok": False, "code": "backend_stopped",
                        "error": "The C++ back end stopped. Reload the page and log in again."}
            return json.loads(reply)

    def stop(self):
        if self.process is not None and self.process.poll() is None:
            try:
                self.process.stdin.write("quit\n")
                self.process.stdin.flush()
                self.process.wait(timeout=2)
            except Exception:
                self.process.kill()


BACKEND = Backend()
BUSY = set()              # tokens with an AI request running (stops double clicks)
BUSY_LOCK = threading.Lock()


class Job:
    """The rest of a session, being written by the AI in the background."""

    def __init__(self, serial, expected):
        self.serial = serial          # which session the questions belong to (from C++ start_ids)
        self.expected = expected      # questions the student will get if every check passes
        self.finished = threading.Event()
        self.cancelled = False


JOBS = {}                 # token -> Job


def fail(code, message):
    return {"ok": False, "code": code, "error": message}


def one_at_a_time(token):
    with BUSY_LOCK:
        if token in BUSY:
            return False
        BUSY.add(token)
        return True


def done(token):
    with BUSY_LOCK:
        BUSY.discard(token)


# ------------------------------------------------------------ AI-backed commands

def import_records(records):
    ids = []
    for record in records:
        saved = BACKEND.call("import_question", record)
        if saved.get("ok"):
            ids.append(str(saved["id"]))
        else:
            print("Question rejected by C++:", saved.get("error"))
    return ids


def write_rest(cfg, prep, token, job, slots):
    """Background thread: writes the remaining questions and adds them to the running session."""
    def on_ready(records):
        if job.cancelled:
            return
        ids = import_records(records)
        if ids:
            added = BACKEND.call("append_ids", [token, str(job.serial), ",".join(ids)])
            if added.get("code") == "stale":       # the student moved on to another session
                job.cancelled = True
    try:
        question_ai.generate_rest(cfg, prep, slots, on_ready, lambda: job.cancelled)
    except Exception as e:
        print(f"Background generation stopped: {e!r}")
    finally:
        job.finished.set()


# ---------------------------------------------------------------- prefetch
#
# Speed: when a student opens the Revision page, we already know their weakest
# topic, so a couple of MCQs for it are written in the background right away.
# If the student then starts revision on that topic, the session begins with
# these at once instead of waiting for the AI. They are kept in memory only
# (not saved) until used, are used once, and expire after PREFETCH_MAX_AGE.

PREFETCH_COUNT = 2
PREFETCH_MAX_AGE = 15 * 60          # seconds
PREFETCH = {}                        # token -> Prefetch
PREFETCH_LOCK = threading.Lock()


class Prefetch:
    def __init__(self, topic):
        self.topic = topic
        self.created = time.time()
        self.records = []
        self.done = threading.Event()

    def fresh(self):
        return time.time() - self.created < PREFETCH_MAX_AGE


def start_prefetch(cfg, token, topic):
    """Starts writing PREFETCH_COUNT MCQs on `topic` unless a fresh one is already there."""
    with PREFETCH_LOCK:
        old = PREFETCH.get(token)
        if old and old.topic == topic and old.fresh() and (not old.done.is_set() or old.records):
            return
        prep = BACKEND.call("prepare", [token, "revision", topic, "*", "All", "MCQ", str(PREFETCH_COUNT)])
        if not prep.get("ok"):
            return
        job = Prefetch(topic)
        PREFETCH[token] = job

    def work():
        job.records = question_ai.prefetch(cfg, prep)
        print(f"Prefetch: {len(job.records)} revision question(s) ready for {topic}.")
        job.done.set()
    threading.Thread(target=work, daemon=True).start()


def take_prefetch(token, topic):
    """Hands over the prefetched records for this topic (once), waiting if they are still
    being written: they were started earlier, so they arrive sooner than a new request."""
    with PREFETCH_LOCK:
        job = PREFETCH.pop(token, None)
    if job is None or job.topic != topic or not job.fresh():
        return []
    job.done.wait(question_ai.FIRST_TIMEOUT)
    return list(job.records)


def cmd_generate(cfg, args):
    """Practice: ONLY from the question set (the team's dataset), never the AI.
    If too few questions match, similar ones of the same topic fill the gap.
    Revision: ONLY from the AI (API key), aimed at the weak subtopics."""
    token = args[0]
    old = JOBS.pop(token, None)          # a new session replaces one still being written
    if old:
        old.cancelled = True
    if args[1] == "practice":
        return BACKEND.call("practice_start", list(args) + ["fill"])
    problem = settings.ai_problem(cfg)
    if problem:
        return fail("ai_not_configured", problem + " (The key goes in config.json on the computer running web_app.py.)")
    prep = BACKEND.call("prepare", args)
    if not prep.get("ok"):
        return prep
    if not one_at_a_time(token):
        return fail("busy", "Your questions are already being prepared.")
    try:
        slots = question_ai.plan_slots(prep)
        cached = take_prefetch(token, args[2])
        mcq = [i for i, slot in enumerate(slots) if slot["type"] == "MCQ"][:len(cached)]
        if mcq:
            # Start at once with the prefetched MCQs; they replace MCQ slots of this session.
            records = cached[:len(mcq)]
            rest = [slot for i, slot in enumerate(slots) if i not in mcq]
            prep["avoid"] = [r[4] for r in records] + list(prep.get("avoid") or [])
            print(f"Revision started with {len(records)} prefetched question(s).")
        else:
            first, rest = question_ai.split_slots(slots)
            records, retry = question_ai.generate_first(cfg, prep, first)
            rest = retry + rest      # failed first-batch slots are written in the background
    except AIError as e:
        return fail("ai_error", str(e))
    finally:
        done(token)
    ids = import_records(records)
    if not ids:
        return fail("ai_error", "The AI's questions didn't pass the checks. Try again.")
    started = BACKEND.call("start_ids", [token, prep["mode"], ",".join(ids)])
    if not started.get("ok"):
        return started
    if rest:
        job = Job(started["serial"], len(ids) + len(rest))
        JOBS[token] = job
        threading.Thread(target=write_rest, args=(cfg, prep, token, job, rest), daemon=True).start()
        started["total"] = job.expected
    started["requested"] = prep["count"]
    started["topic"] = prep["topics"][0]["name"] if len(prep["topics"]) == 1 else ""
    return started


def with_pending(token, reply):
    """While background questions are still coming, show the expected total,
    so the page doesn't say "See results" too early."""
    job = JOBS.get(token)
    if not reply.get("ok") or job is None or job.cancelled:
        return reply
    if job.finished.is_set():
        JOBS.pop(token, None)
        return reply
    reply["total"] = max(reply.get("total", 0), job.expected)
    if "remaining" in reply:
        reply["remaining"] = max(reply["remaining"], job.expected - reply.get("attempted", 0))
    reply["pending"] = True
    return reply


def cmd_plan(cfg, args):
    """The revision ranking from C++, plus whether the AI can write revision questions."""
    reply = BACKEND.call("plan", args)
    if reply.get("ok"):
        problem = settings.ai_problem(cfg)
        reply["aiReady"] = not problem
        reply["aiProblem"] = problem or ""
        if not problem and reply.get("hasData") and reply.get("weakest"):
            start_prefetch(cfg, args[0], reply["weakest"])
    return reply


def cmd_review(cfg, args):
    job = JOBS.pop(args[0], None)       # a new session replaces the one still being written
    if job:
        job.cancelled = True
    return BACKEND.call("review", args)


def cmd_next(cfg, args):
    token = args[0]
    reply = BACKEND.call("next", args)
    job = JOBS.get(token)
    if reply.get("ok") and reply.get("done") and job and not job.cancelled and not job.finished.is_set():
        # The student is faster than the AI: wait for the next questions.
        while not job.finished.wait(1):
            reply = BACKEND.call("next", args)
            if not reply.get("done"):
                return with_pending(token, reply)
        reply = BACKEND.call("next", args)
    return with_pending(token, reply)


def cmd_answer(cfg, args):
    token, answer = args
    info = BACKEND.call("current_info", [token])
    if not info.get("ok") or not info.get("needsAi"):
        return with_pending(token, BACKEND.call("answer", args))
    if not answer.strip():
        return fail("invalid_answer", "Write your code first.")
    problem = settings.ai_problem(cfg)
    if problem:
        return fail("ai_not_configured", problem)
    if not one_at_a_time(token):
        return fail("busy", "Your code is already being checked.")
    try:
        correct, feedback = question_ai.grade_code(cfg, info, answer)
    except AIError as e:
        return fail("ai_error", str(e) + " Your answer was not saved; press Check again.")
    finally:
        done(token)
    return with_pending(token, BACKEND.call("answer_graded", [token, "1" if correct else "0", answer, feedback]))


def cmd_explain_mistake(cfg, args):
    token, question_id = args
    info = BACKEND.call("mistake_info", [token, question_id])
    if not info.get("ok"):
        return info
    if info.get("note"):
        return {"ok": True, "note": info["note"], "saved": True}
    problem = settings.ai_problem(cfg)
    if problem:
        return fail("ai_not_configured", problem)
    try:
        note = question_ai.explain_mistake(cfg, info)
    except AIError as e:
        return fail("ai_error", str(e))
    BACKEND.call("save_note", [token, question_id, note])
    return {"ok": True, "note": note, "saved": False}


def cmd_forgot_start(cfg, args):
    role, email = args[0], args[1].strip().lower()
    if role not in ("student", "admin"):
        return fail("bad_request", "Choose student or admin.")
    if "@" not in email or len(email) > 80:
        return fail("bad_request", "Enter a valid email address.")
    code = f"{secrets.randbelow(1_000_000):06d}"   # cryptographically secure
    started = BACKEND.call("otp_start", [role, email, code])
    generic = {"ok": True, "message": "If an account with this email exists, a 6-digit OTP has been sent to it. "
                                      "It works for 10 minutes. Check your spam folder too."}
    if not started.get("ok"):
        # Same reply whether or not the account exists, so this form can't be
        # used to find out which emails are registered.
        return started if started.get("code") == "cooldown" else generic
    if not settings.email_ready(cfg):
        print(f"\n[EMAIL NOT SET UP] OTP for {role} {email}: {code}  (valid {started['validMinutes']} min)\n")
        return {"ok": True, "devMode": True,
                "message": "Email sending isn't set up on this computer yet, so the OTP was printed "
                           "in the server window (the terminal running web_app.py)."}
    try:
        mailer.send_otp(cfg, email, started["name"], code, started["validMinutes"], role)
    except mailer.MailError as e:
        return fail("mail_error", str(e))
    return generic


PYTHON_COMMANDS = {
    "generate": cmd_generate,
    "next": cmd_next,
    "review": cmd_review,
    "plan": cmd_plan,
    "answer": cmd_answer,
    "explain_mistake": cmd_explain_mistake,
    "forgot_start": cmd_forgot_start,
}


class Handler(BaseHTTPRequestHandler):
    def _send(self, status, body, content_type):
        data = body.encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(data)

    def _send_json(self, obj, status=200):
        self._send(status, json.dumps(obj), "application/json; charset=utf-8")

    def do_GET(self):
        if self.path in ("/", "/index.html"):
            try:
                with open(PAGE_PATH, encoding="utf-8") as f:
                    self._send(200, f.read(), "text/html; charset=utf-8")
            except FileNotFoundError:
                self._send(500, "web/index.html is missing.", "text/plain; charset=utf-8")
        else:
            self._send(404, "Not found", "text/plain; charset=utf-8")

    def do_POST(self):
        if self.path != "/api":
            return self._send(404, "Not found", "text/plain; charset=utf-8")
        try:
            length = int(self.headers.get("Content-Length", "0"))
            if length <= 0 or length > MAX_BODY_BYTES:
                raise ValueError("bad length")
            request = json.loads(self.rfile.read(length).decode("utf-8"))
            command = request.get("cmd")
            args = request.get("args", [])
            if command not in ALLOWED_COMMANDS:
                raise ValueError("unknown command")
            if not isinstance(args, list) or len(args) != ALLOWED_COMMANDS[command]:
                raise ValueError("wrong arguments")
            limit = MAX_ANSWER_LENGTH if command == "answer" else MAX_ARG_LENGTH
            if any(not isinstance(a, (str, int)) or len(str(a)) > limit for a in args):
                raise ValueError("bad argument")
            args = [str(a) for a in args]
        except (ValueError, json.JSONDecodeError, UnicodeDecodeError):
            return self._send_json(fail("bad_request", "The request was not understood."), 400)

        handler = PYTHON_COMMANDS.get(command)
        if handler is None:
            return self._send_json(BACKEND.call(command, args))
        try:
            self._send_json(handler(settings.load(), args))
        except Exception as e:   # never leave the browser waiting
            print(f"Error in {command}: {e!r}")
            self._send_json(fail("server_error", "Something went wrong on the server. Try again."))

    def log_message(self, fmt, *args):
        pass


def start_server():
    for port in range(FIRST_PORT, FIRST_PORT + 10):
        try:
            server = ThreadingHTTPServer((HOST, port), Handler)
            server.daemon_threads = True
            return server, port
        except OSError:
            continue
    print(f"Ports {FIRST_PORT}-{FIRST_PORT + 9} are all in use.")
    sys.exit(1)


def prepare_dataset():
    """Imports the question sheet from dataset/ when it is new or has changed,
    so nobody has to run import_dataset.py by hand."""
    if "SRP_DATASET" in os.environ:          # test_web.py makes its own question set
        return
    import import_dataset                    # here, not at the top: import_dataset imports this file
    txt = os.path.join(PROJECT_DIR, DATASET_FILE)
    source = import_dataset.find_source()
    if source is None:
        if not os.path.exists(txt):
            print(import_dataset.explain_missing())
        return
    if os.path.exists(txt) and os.path.getmtime(txt) >= os.path.getmtime(source):
        return                               # already imported, nothing changed
    print(f"Importing the question set from dataset/{os.path.basename(source)} ...")
    subprocess.run([sys.executable, os.path.join(PROJECT_DIR, "import_dataset.py"), source, "--brief"],
                   cwd=PROJECT_DIR)


def main():
    os.makedirs(os.path.join(PROJECT_DIR, DATA_DIR), exist_ok=True)
    if needs_build() and not build():
        sys.exit(1)
    prepare_dataset()
    hello = BACKEND.call("hello", [])         # starts C++ now, so it prints the question-set summary
    if hello.get("datasetQuestions", 0) > 0:
        print(f"Practice: ready ({hello['datasetQuestions']} questions)")
    else:
        print("Practice: NOT READY - no questions loaded. Put your question sheet in the dataset folder "
              "and start the website again.")
    cfg = settings.load()
    problem = settings.ai_problem(cfg)
    print("AI (revision, code grading, explanations): " +
          (f"{cfg['ai']['provider']} ({cfg['ai']['model']})" if not problem else "NOT READY - " + problem))
    print("OTP email: " + ("ready" if settings.email_ready(cfg) else
                           "not set up - OTPs will be printed in this window"))
    server, port = start_server()
    url = f"http://{HOST}:{port}"
    print(f"Smart Revision Platform is running at {url}")
    print("Press Ctrl+C to stop.")
    if "--no-browser" not in sys.argv:
        threading.Timer(0.8, lambda: webbrowser.open(url)).start()
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping...")
    finally:
        server.server_close()
        BACKEND.stop()


if __name__ == "__main__":
    main()
