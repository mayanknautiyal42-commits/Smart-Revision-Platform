#!/usr/bin/env python3
"""
test_web.py - tests the whole website flow offline, with a FAKE AI.

    python test_web.py

No API key, internet or email is needed: the AI calls are replaced by a
fake that returns fixed questions, and OTPs are read from the server. The
test uses its own folder (data_test/), so real student data is untouched.
It checks: the question-set importer (CSV -> dataset file, with the g++
check of output answers), sign-up with email, separate student/admin login,
forgot password with OTP, practice from the question set (all 4 types, no
AI needed), AI revision sessions, AI grading of code, mistakes and their
explanations, reports, the revision plan and the admin overview.
"""

import csv
import json
import os
import re
import shutil
import sys
import threading
import time
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
os.environ["SRP_DATA_DIR"] = "data_test"
os.environ["SRP_DATASET"] = os.path.join("data_test", "dataset.txt")
shutil.rmtree(os.path.join(HERE, "data_test"), ignore_errors=True)
os.makedirs(os.path.join(HERE, "data_test"))

import import_dataset  # noqa: E402
import question_ai  # noqa: E402
import settings     # noqa: E402
import web_app      # noqa: E402

failures = 0


def check(condition, name):
    global failures
    print(("[PASS] " if condition else "[FAIL] ") + name)
    if not condition:
        failures += 1


# ------------------------------------------------------------ retries when the AI is busy (HTTP 503)
import ai_client  # noqa: E402

import types  # noqa: E402
ai_client.time = types.SimpleNamespace(sleep=lambda seconds: None)   # no real waiting between retries
CFG = {"ai": {"provider": "gemini", "api_key": "k", "model": "main", "fallback_model": "backup", "base_url": ""}}


def scripted(replies):
    """A fake Gemini call that returns the given replies in order; BusyError = HTTP 503."""
    used = []

    def call(ai, system, prompt, max_tokens, timeout):
        used.append(ai["model"])
        reply = replies.pop(0)
        if reply == "503":
            raise ai_client.BusyError("busy")
        if reply == "401":
            raise ai_client.AIError("The AI provider rejected the API key.")
        return reply
    return call, used


call, used = scripted(["503", "503", '{"ok": 1}'])
ai_client.CALLERS["gemini"] = call
check(ai_client.complete_json(CFG, "s", "p") == {"ok": 1} and used == ["main"] * 3,
      "AI busy (503) twice: the third try works")
call, used = scripted(["503", "503", "503", '{"ok": 2}'])
ai_client.CALLERS["gemini"] = call
check(ai_client.complete_json(CFG, "s", "p") == {"ok": 2} and used == ["main"] * 3 + ["backup"],
      "AI busy 3 times: the backup model answers")
call, used = scripted(["503"] * 6)
ai_client.CALLERS["gemini"] = call
try:
    ai_client.complete_json(CFG, "s", "p")
    check(False, "Both models busy: a clear error is shown")
except ai_client.BusyError as e:
    check("busy" in str(e) and len(used) == 6, "Both models busy: a clear error is shown")
call, used = scripted(["401"])
ai_client.CALLERS["gemini"] = call
try:
    ai_client.complete_json(CFG, "s", "p")
except ai_client.AIError as e:
    check("rejected" in str(e) and used == ["main"], "A wrong key is not retried")

# hedged request: a slow first try gets a second copy, and the faster reply wins
SLOW = {"n": 0}


def slow_then_fast(ai, system, prompt, max_tokens, timeout):
    SLOW["n"] += 1
    if SLOW["n"] == 1:
        time.sleep(3)
        return '{"copy": 1}'
    return '{"copy": 2}'


ai_client.CALLERS["gemini"] = slow_then_fast
began = time.time()
got = ai_client.complete_json(CFG, "s", "p", hedge_after=0.5)
check(got == {"copy": 2} and time.time() - began < 2 and SLOW["n"] == 2,
      "Hedged request: a slow reply is overtaken by a second copy")
SLOW["n"] = 1                     # next call is fast: no second copy is sent
check(ai_client.complete_json(CFG, "s", "p", hedge_after=0.5) == {"copy": 2} and SLOW["n"] == 2,
      "Hedged request: a fast reply sends no extra request")
ai_client.CALLERS["gemini"] = ai_client._gemini

# thinking level refused (HTTP 400) -> asked again without it
sent = []


def fake_post(url, headers, body, timeout):
    sent.append("thinkingConfig" in body["generationConfig"])
    if sent[-1]:
        raise ai_client.AIError("bad", 400)
    return {"candidates": [{"content": {"parts": [{"text": '{"ok": 3}'}]}}]}


real_post = ai_client._post
ai_client._post = fake_post
cfg2 = {"ai": dict(CFG["ai"], thinking="low")}
check(ai_client.complete_json(cfg2, "s", "p") == {"ok": 3} and sent == [True, False],
      "A model that refuses the thinking setting is asked again without it")
ai_client.complete_json(cfg2, "s", "p")
check(sent == [True, False, False], "...and the setting is not sent to that model again")
ai_client._post = real_post

# first batch = the fastest question type, so the student waits for one short reply
mixed = [{"type": t, "topic": "A", "subtopic": "a", "level": "Easy"} for t in ["Code", "Output", "MCQ", "Code", "MCQ", "Output"]]
first, rest = question_ai.split_slots(mixed)
check([s["type"] for s in first] == ["MCQ"] and len(rest) == 5,
      "Mixed revision: the session starts with the fastest type (MCQ)")

# ------------------------------------------------------------ fake AI
CALLS = {"output": 0, "all": 0}
DELAY = {"seconds": 0}    # makes the fake AI slow, to test background writing


def slot_count(prompt):
    return len(re.findall(r"^\d+\. Topic: ", prompt, re.M))


AI_DOWN = {"on": False}


def fake_complete_json(cfg, system, prompt, max_tokens=8000, timeout=150, hedge_after=None):
    CALLS["all"] += 1
    time.sleep(DELAY["seconds"])
    if AI_DOWN["on"]:
        raise question_ai.AIError("The AI is busy (test).")
    n = slot_count(prompt)
    if "multiple-choice" in prompt:
        return {"questions": [{"question": f"Fake MCQ {i} ({prompt.count('Slots')})", "code": "",
                               "options": ["right", "wrong a", "wrong b", "wrong c"], "answer": 1,
                               "explanation": "Because right is right."} for i in range(n)]}
    if "predict the output" in prompt:
        CALLS["output"] += 1
        items = []
        for i in range(n):
            code = "#include <iostream>\nusing namespace std;\nint main() {\n    int s[3] = {1, 2, 3};\n" \
                   f"    cout << s[2] - s[0] + {i};\n    return 0;\n}}"
            # The very first item claims a wrong output: it must be thrown away by the g++ check.
            wrong = CALLS["output"] == 1 and i == 0
            items.append({"question": "What does this program print?", "code": code,
                          "output": "999" if wrong else str(2 + i), "explanation": "3 - 1 = 2, plus i."})
        return {"questions": items}
    if "coding exercises" in prompt:
        return {"questions": [{"question": f"Write int top(int s[], int n) returning the top of a stack ({i}).",
                               "starter_code": "int top(int s[], int n) {\n}",
                               "solution": "int top(int s[], int n) { return s[n - 1]; }",
                               "explanation": "The top is the last filled index. O(1)."} for i in range(n)]}
    if "Grade a student's answer" in prompt:
        student = prompt.split("<<<STUDENT_CODE", 1)[1]
        ok = "n - 1" in student
        return {"correct": ok, "feedback": "Good." if ok else "You returned s[n]; the top is at n - 1."}
    if "answered this question wrong" in prompt:
        return {"note": "Why your answer is wrong: test.\nThe idea to remember: test.\n"
                        "How to avoid it next time: test.\nQuick check: test (yes)"}
    raise AssertionError("unexpected prompt")


question_ai.complete_json = fake_complete_json
AI_READY = lambda: {"ai": {"provider": "gemini", "api_key": "fake", "model": "fake", "fallback_model": "",
                                "base_url": ""},
                         "email": {"smtp_host": "", "smtp_port": 587, "username": "", "password": "",
                                   "from_name": "x"}}
settings.load = AI_READY
OTPS = []
web_app.secrets.randbelow = lambda n: OTPS.append(n) or 123456   # predictable OTP for the test


# ------------------------------------------------------------ the question set (importer)
def program(value):
    return ("#include <iostream>\nusing namespace std;\nint main() {\n    int s[3] = {1, 2, 3};\n"
            f"    cout << s[2] - s[0] + {value};\n    return 0;\n}}")


HEADER = ["ID", "Type", "Topic", "Sub Topic", "Difficulty", "Question", "Code",
          "Option A", "Option B", "Option C", "Option D", "Answer", "Explanation"]
ROWS = []
ARRAY_SUBS = ["Address Calculation", "Sparse Matrices", "Applications of Arrays"]
for i in range(12):            # answers written three ways: letter, number, option text
    ROWS.append([i + 1, "MCQ", "Arrays", ARRAY_SUBS[i % 3], ["Easy", "Medium", "Hard"][i % 3], f"Arrays Q{i}", "",
                 "right", "wrong a", "wrong b", "wrong c", ["A", "1", "right"][i % 3], "Because right is right."])
for i in range(4):             # the first one claims a wrong output: the g++ check must refuse it
    ROWS.append([20 + i, "Predict output", "Stacks", "Stack Operations", "easy", "", program(i),
                 "", "", "", "", "999" if i == 0 else str(2 + i), "3 - 1 = 2, plus i."])
for i in range(2):
    ROWS.append([30 + i, "Code", "Stacks", "Array Implementation of Stack", "Medium",
                 f"Write int top(int s[], int n) returning the top of a stack ({i}).", "int top(int s[], int n) {\n}",
                 "", "", "", "", "int top(int s[], int n) { return s[n - 1]; }", "The top is the last filled index."])
for i in range(3):
    ROWS.append([40 + i, "MCQ", "Queues", "Circular Queue", "Easy", f"Queue Q{i}", "", "right", "w1", "w2", "w3", "A", "x"])
for i in range(2):
    ROWS.append([50 + i, "MCQ", "Sorting", "Bubble Sort", "Medium", f"Sort Q{i}", "", "right", "w1", "w2", "w3", "1", "x"])
ROWS.append([60, "MCQ", "graphs", "dijkstra's algorithm", "Hard", "Graph Q", "", "right", "w1", "w2", "w3", "A", "x"])
ROWS.append([61, "MCQ", "Trees", "Binary Trees", "Easy", "Bad Q", "", "x", "y", "z", "All of the above", "D", "x"])
ROWS.append([62, "MCQ", "Trees", "Binary Tree", "Medium", "Typo Q", "", "a", "b", "c", "d", "A", "x"])
ROWS.append([1, "MCQ", "Arrays", "Sparse Matrices", "Easy", "Reused id", "", "a", "b", "c", "d", "A", "x"])
GOOD = 12 + 3 + 2 + 3 + 2 + 1

csv_path = os.path.join(HERE, "data_test", "questions.csv")
with open(csv_path, "w", newline="", encoding="utf-8-sig") as f:      # utf-8-sig: what Excel writes
    csv.writer(f).writerows([HEADER] + ROWS)
rows, _ = import_dataset.read_rows(csv_path)
records, problems, _ = import_dataset.convert(rows, import_dataset.load_syllabus(), progress=False)
check(len(rows) == len(ROWS) and len(records) == GOOD, f"Importer: {GOOD} good rows kept from {len(ROWS)}")
wrong_output_row = 2 + 12           # Excel row of the first Output question
check("prints \"2\"" in problems.get(wrong_output_row, [""])[0], "Importer: g++ refuses a wrong output answer")
check(any("All of the above" in m for m in problems.get(2 + 12 + 4 + 2 + 3 + 2 + 1, [])),
      "Importer: refuses 'All of the above' (options are shuffled)")
check(any('Did you mean "Binary Trees"' in m for m in problems.get(len(ROWS), [])), "Importer: suggests the right subtopic")
check(any("already used" in m for m in problems.get(len(ROWS) + 1, [])), "Importer: refuses a reused id")
check(any(r["topic"] == "Graphs" and r["subtopic"] == "Dijkstra's Algorithm" for r in records),
      "Importer: fixes capitals in topic names")
import_dataset.write_dataset(records, os.path.join(HERE, "data_test", "dataset.txt"))

finder = os.path.join(HERE, "data_test", "finder")
os.makedirs(finder)
check(import_dataset.find_source(finder) is None, "Importer: no sheet in the folder is reported, not guessed")
for name in ("questions_template.csv", "~$DSA.xlsx", "DSA questions.csv.csv"):
    open(os.path.join(finder, name), "w").close()
check(os.path.basename(import_dataset.find_source(finder) or "") == "DSA questions.csv.csv",
      "Importer: finds a sheet with any name (not the template or Excel's lock file)")


# ------------------------------------------------------------ server
if web_app.needs_build() and not web_app.build():
    sys.exit(1)
server, port = web_app.start_server()
threading.Thread(target=server.serve_forever, daemon=True).start()
URL = f"http://127.0.0.1:{port}/api"


def api(cmd, *args):
    body = json.dumps({"cmd": cmd, "args": list(args)}).encode()
    req = urllib.request.Request(URL, data=body, headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=120) as r:
            return json.loads(r.read())
    except urllib.error.HTTPError as e:
        return json.loads(e.read())


def run_session(token, answer_for):
    """Answers every question of the current session. Returns (correct, total, types seen)."""
    right, total, types = 0, 0, set()
    while True:
        d = api("next", token)
        if d["done"]:
            return right, total, types
        q = d["question"]
        types.add(q["type"])
        if total == 0:
            allowed = {"id", "topic", "subtopic", "difficulty", "type", "text", "code", "options"}
            check(set(q) <= allowed, f"{q['type']} question reaches the browser without its answer key")
        a = api("answer", token, answer_for(q))
        total += 1
        right += a["correct"]


try:
    # ---------- accounts
    check(api("hello")["needsSetup"], "First run asks for admin setup")
    check(len(api("hello")["topics"]) == 11, "All 11 syllabus topics are listed")
    admin = api("setup", "Ms Rawat", "Teacher@College.edu", "teach123")
    check(admin["ok"] and admin["email"] == "teacher@college.edu", "Admin created with email (lower-cased)")
    check(not api("register", "Ridam", "not-an-email", "pass123")["ok"], "Sign-up refuses an invalid email")
    check(not api("register", "Ridam", "ridam@gmail.com", "123")["ok"], "Sign-up refuses a short password")
    st = api("register", "Ridam Sharma", "ridam@gmail.com", "pass123")
    check(st["ok"] and st["role"] == "student", "Student signs up with full name + email")
    check(not api("register", "Copy", "RIDAM@gmail.com", "pass123")["ok"], "Same email can't sign up twice")
    bad = api("login", "admin", "ridam@gmail.com", "pass123")
    check(not bad["ok"] and "Student login" in bad["error"], "Student account can't use the Admin login")
    bad = api("login", "student", "teacher@college.edu", "teach123")
    check(not bad["ok"] and "Admin login" in bad["error"], "Admin account can't use the Student login")
    check(api("login", "student", "ridam@gmail.com", "pass123")["ok"], "Student logs in with email")
    check(api("admin_overview", st["token"])["code"] == "forbidden", "Student token can't open admin pages")
    check(api("otp_start", "student", "ridam@gmail.com", "000000").get("code") == "bad_request",
          "Browser can't call internal commands (otp_start)")

    # ---------- forgot password
    d = api("forgot_start", "student", "nobody@gmail.com")
    check(d["ok"] and not d.get("devMode"), "Unknown email gets the same neutral reply")
    d = api("forgot_start", "student", "ridam@gmail.com")
    check(d["ok"] and d.get("devMode"), "Known email: OTP issued (printed, as email isn't configured)")
    check(api("forgot_start", "student", "ridam@gmail.com")["code"] == "cooldown", "Resend is limited to once a minute")
    check(not api("reset_password", "admin", "ridam@gmail.com", "123456", "newpass1")["ok"],
          "Student OTP can't reset through the admin form")
    w = api("reset_password", "student", "ridam@gmail.com", "111111", "newpass1")
    check(not w["ok"] and "4 tries" in w["error"], "Wrong OTP counts down the tries")
    check(api("reset_password", "student", "ridam@gmail.com", "123456", "newpass1")["ok"], "Right OTP changes the password")
    check(not api("reset_password", "student", "ridam@gmail.com", "123456", "again12")["ok"], "An OTP works only once")
    check(api("syllabus", st["token"])["code"] == "login_required", "Old logins end after a password reset")
    check(not api("login", "student", "ridam@gmail.com", "pass123")["ok"], "Old password stops working")
    st = api("login", "student", "ridam@gmail.com", "newpass1")
    check(st["ok"], "New password works")
    T = st["token"]
    d = api("forgot_start", "admin", "teacher@college.edu")
    check(d["ok"] and d.get("devMode"), "Admin can also ask for an OTP")

    # ---------- syllabus has no question counts
    syl = api("syllabus", T)
    check(all("easy" not in t and "count" not in t for t in syl["topics"]), "Topics are sent without question counts")
    arrays = next(t for t in syl["topics"] if t["name"] == "Arrays")
    check("Sparse Matrices" in arrays["subtopics"], "Subtopics come from the syllabus")

    # ---------- practice comes ONLY from the question set; revision ONLY from the AI
    check(not api("generate", T, "practice", "Arrays", "*", "All", "MCQ", "21")["ok"], "More than 20 questions is refused")
    g = api("generate", T, "practice", "Arrays", "*", "All", "MCQ", "12")
    check(g["ok"] and g["total"] == 12 and g["source"] == "dataset", "12 Arrays MCQs, all from the set")
    first_ids = []
    right, total, types = run_session(T, lambda q: first_ids.append(q["id"]) or str(q["options"].index("right") + 1))
    check(total == 12 and right == 12 and types == {"MCQ"}, "All 12 MCQs answered (options were shuffled)")
    check(all(i > 500000 for i in first_ids), "...and every one came from the question set")
    ai_calls = CALLS["all"]

    g = api("generate", T, "practice", "Stacks", "Stack Operations", "Easy", "Output", "3")
    check(g["ok"] and g["total"] == 3, "Output questions from the set (the wrong one never got in)")
    right, total, _ = run_session(T, lambda q: "0")
    check(total == 3 and right == 0, "Wrong output answers are marked wrong")
    check(CALLS["all"] == ai_calls, "Practice MCQ and output sessions don't call the AI")

    g = api("generate", T, "practice", "Stacks", "*", "Medium", "Code", "2")
    check(g["ok"] and g["total"] == 2, "Code questions from the set")
    answers = iter(["int top(int s[], int n) { return s[n - 1]; }", "int top(int s[], int n) { return s[n]; }"])
    right, total, _ = run_session(T, lambda q: next(answers))
    check(total == 2 and right == 1, "AI grading marks one right, one wrong")

    g = api("generate", T, "practice", "*", "*", "All", "Mixed", "6")
    check(g["ok"] and g["total"] == 6, "Mixed session across all topics")
    right, total, types = run_session(T, lambda q: {"MCQ": "1", "Output": "x",
                                                    "Code": "int top(int s[], int n) { return 0; }"}[q["type"]])
    check(types == {"MCQ", "Output", "Code"}, "Mixed has MCQ, predict output and code questions")

    ai_calls = CALLS["all"]
    g = api("generate", T, "practice", "Sorting", "*", "All", "MCQ", "6")
    ids = []
    run_session(T, lambda q: ids.append(q["id"]) or "1")
    check(g["ok"] and g["total"] == 2 and g["requested"] == 6 and all(i > 500000 for i in ids),
          "Only 2 Sorting questions in the set: 2, never AI questions")
    g = api("generate", T, "practice", "Stacks", "*", "Easy", "MCQ", "4")
    seen = []
    run_session(T, lambda q: seen.append((q["topic"], q["type"])) or "1")
    check(g["ok"] and g["total"] == 3 and g["similar"] == 3 and set(seen) == {("Stacks", "Output")},
          "No Stacks MCQs: the 3 Stacks output questions from the set fill in (never code)")
    nothing = api("generate", T, "practice", "Recursion", "*", "Medium", "MCQ", "3")
    check(not nothing["ok"] and nothing["code"] == "no_questions" and "Recursion" in nothing["error"],
          "Nothing on the topic in the set: a clear message (the AI is not used)")
    check(CALLS["all"] == ai_calls, "Practice never asks the AI to write questions")

    settings.load = lambda: {"ai": {"provider": "gemini", "api_key": "", "model": "", "fallback_model": "",
                                    "base_url": ""}, "email": AI_READY()["email"]}
    g = api("generate", T, "practice", "Queues", "*", "All", "MCQ", "3")
    check(g["ok"] and g["total"] == 3, "Practice works with no API key")
    run_session(T, lambda q: "1")
    plan = api("plan", T)
    check(plan["ok"] and plan["aiReady"] is False, "Revision page knows the AI is not set up")
    check(api("generate", T, "revision", "*", "*", "All", "MCQ", "3")["code"] == "ai_not_configured",
          "...but revision needs the API key")
    settings.load = AI_READY

    # ---------- mistakes
    m = api("mistakes", T)
    check(m["ok"] and m["count"] >= 4, "Wrong answers appear in My mistakes")
    first = m["mistakes"][0]
    check(first["myAnswer"] != "" and first["correctAnswer"] != "", "Each mistake shows your answer and the right one")
    code_m = next(x for x in m["mistakes"] if x["question"]["type"] == "Code")
    check("n - 1" in code_m["feedback"], "Code mistakes keep the AI grader's feedback")
    e = api("explain_mistake", T, str(first["question"]["id"]))
    check(e["ok"] and "Why your answer is wrong" in e["note"] and not e["saved"], "AI explains a mistake")
    e2 = api("explain_mistake", T, str(first["question"]["id"]))
    check(e2["ok"] and e2["saved"], "The explanation is saved and reused")
    r = api("review", T, "20")
    check(r["ok"] and r["total"] == min(20, m["count"]), "Mistakes can be practised again")
    api("next", T)

    # ---------- report and plan
    rep = api("report", T)["report"]
    stacks = next(t for t in rep["topics"] if t["name"] == "Stacks")
    check(stacks["attempts"] >= 5 and len(stacks["subtopics"]) >= 1, "Report: topic-wise data with subtopics")
    check(len(rep["timeline"]) == 1 and rep["timeline"][0]["attempts"] == rep["summary"]["attempts"],
          "Report: day-wise data for the progress graph")
    check({t["name"] for t in rep["types"]} == {"MCQ", "Output", "Code"}, "Report: all 3 question types")
    plan = api("plan", T)
    lowest = min(t["accuracy"] for t in rep["topics"] if t["attempts"])
    weakest = next(t for t in rep["topics"] if t["name"] == plan["weakest"])
    check(plan["hasData"] and weakest["accuracy"] == lowest, "Plan: weakest topic found with the min-heap")
    g = api("generate", T, "revision", "*", "*", "All", "MCQ", "4")
    check(g["ok"] and g["total"] == 4 and g["topic"] == plan["weakest"], "Revision session targets the weakest topic")
    run_session(T, lambda q: "1")
    accs = [r["accuracy"] for r in plan["ranking"]]
    check(plan["aiReady"] is True and accs == sorted(accs) and plan["ranking"][0]["topic"] == plan["weakest"],
          "Revision page: topics weakest first (min-heap order), AI ready")
    check(all(isinstance(r["weakSubtopics"], list) for r in plan["ranking"])
          and plan["ranking"][0]["weakSubtopics"], "Revision page: weak spots for every topic")
    g = api("generate", T, "revision", "Arrays", "*", "All", "MCQ", "6")
    ids = []
    right, total, _ = run_session(T, lambda q: ids.append(q["id"]) or str(q["options"].index("right") + 1))
    check(g["ok"] and total == 6 and all(i < 500000 for i in ids),
          "Revision questions all come from the AI (none from the question set)")
    AI_DOWN["on"] = True
    g = api("generate", T, "revision", "Arrays", "*", "All", "MCQ", "6")
    check(not g["ok"] and g["code"] == "ai_error", "AI down: revision says so (no set questions instead)")
    AI_DOWN["on"] = False
    strongest = plan["ranking"][-1]["topic"]
    g = api("generate", T, "revision", strongest, "*", "All", "Mixed", "3")
    topics = []
    run_session(T, lambda q: topics.append(q["topic"]) or "1")
    check(g["ok"] and g["total"] == 3 and set(topics) == {strongest}, "Revision button works for any topic in the list")

    # ---------- speed: the session starts before every question is written
    DELAY["seconds"] = 1.0
    began = time.time()
    g = api("generate", T, "revision", "Sorting", "*", "All", "MCQ", "20")
    waited = time.time() - began
    job = web_app.JOBS.get(T)
    check(g["ok"] and g["total"] == 20 and job is not None and not job.finished.is_set(),
          f"20 AI revision MCQs: the session starts after the first batch ({waited:.1f} s), the rest is written in the background")
    seen, early_end = 0, False
    while True:
        d = api("next", T)
        if d["done"]:
            break
        a = api("answer", T, str(d["question"]["options"].index("right") + 1))
        seen += 1
        if a["remaining"] == 0 and seen < 20:
            early_end = True
    check(seen == 20 and not early_end, "A fast student gets all 20, and never sees 'See results' too early")

    g = api("generate", T, "revision", "Queues", "*", "All", "MCQ", "12")
    old_job = web_app.JOBS.get(T)
    p = api("generate", T, "practice", "Sorting", "*", "All", "MCQ", "5")
    old_job.finished.wait(30)
    check(old_job.cancelled and p["ok"] and api("next", T)["total"] == p["total"],
          "Starting practice stops the old revision questions from joining it")
    while not api("next", T)["done"]:
        pass

    g = api("generate", T, "revision", "Queues", "*", "All", "MCQ", "12")
    old_job = web_app.JOBS.get(T)
    r = api("review", T, "3")
    old_job.finished.wait(30)
    d = api("next", T)
    check(r["ok"] and r["total"] == 3 and d["total"] == 3 and d.get("pending") is None,
          "Starting a review stops the old background questions from joining it")
    while not api("next", T)["done"]:
        pass
    DELAY["seconds"] = 0

    # ---------- prefetch: opening the Revision page writes questions before Start
    plan = api("plan", T)
    job = web_app.PREFETCH.get(T)
    check(job is not None and job.topic == plan["weakest"] and job.done.wait(10) and len(job.records) == 2,
          "Opening Revision prefetches 2 MCQs for the weakest topic")
    DELAY["seconds"] = 3.0            # a slow AI: only the prefetch can make Start fast
    began = time.time()
    g = api("generate", T, "revision", plan["weakest"], "*", "All", "Mixed", "6")
    waited = time.time() - began
    check(g["ok"] and g["total"] == 6 and waited < 1.5 and T not in web_app.PREFETCH,
          f"Revision Start uses the prefetched questions ({waited:.1f} s with a 3 s AI), and only once")
    qs = []
    run_session(T, lambda q: qs.append(q["type"]) or "1")
    check(len(qs) == 6 and sorted(qs) == ["Code", "Code", "MCQ", "MCQ", "Output", "Output"],
          "...the rest of the Mixed session still arrives in the background, with the right type mix")
    DELAY["seconds"] = 0
    api("plan", T)
    web_app.PREFETCH[T].done.wait(10)
    other = next(r["topic"] for r in plan["ranking"] if r["topic"] != plan["weakest"])
    g = api("generate", T, "revision", other, "*", "All", "MCQ", "2")
    run_session(T, lambda q: "1")
    check(g["ok"] and T not in web_app.PREFETCH, "Another topic: the prefetch is not used (and is dropped)")

    # ---------- persistence: restart the C++ back end
    rep = api("report", T)["report"]
    explained = str(first["question"]["id"])
    web_app.BACKEND.stop()
    web_app.BACKEND.process = None
    st2 = api("login", "student", "ridam@gmail.com", "newpass1")
    rep2 = api("report", st2["token"])["report"]
    check(rep2["summary"]["attempts"] == rep["summary"]["attempts"], "Answers survive a restart")
    e3 = api("explain_mistake", st2["token"], explained)
    check(e3["ok"] and e3["saved"], "Saved explanations survive a restart")

    # ---------- admin
    ad = api("login", "admin", "teacher@college.edu", "teach123")
    ov = api("admin_overview", ad["token"])
    check(ov["ok"] and ov["studentCount"] == 1 and ov["datasetQuestions"] == GOOD and ov["questionsGenerated"] >= 24,
          "Admin overview: question-set and AI question counts")
    one = api("admin_student", ad["token"], "ridam@gmail.com")
    check(one["ok"] and one["report"]["summary"]["email"] == "ridam@gmail.com", "Admin opens a student by email")
finally:
    server.shutdown()
    web_app.BACKEND.stop()
    shutil.rmtree(os.path.join(HERE, "data_test"), ignore_errors=True)

print("\nAll tests passed." if failures == 0 else f"\n{failures} test(s) FAILED.")
sys.exit(1 if failures else 0)
