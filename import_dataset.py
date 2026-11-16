#!/usr/bin/env python3
"""
import_dataset.py - turns the team's question spreadsheet into the question set
that Practice uses.

    python import_dataset.py                       reads the sheet in the dataset folder
    python import_dataset.py my_questions.csv      reads another file
    python import_dataset.py --no-verify           skips the g++ check (faster)
    python import_dataset.py --brief               short report (used by web_app.py at start-up)

You normally don't need to run this: web_app.py runs it by itself when the
sheet in dataset/ is new or has changed.

Which file: dataset/questions.csv if it exists, otherwise any other .csv or
.xlsx in dataset/ (not the template); if there are several, the newest one.
Both .csv ("CSV UTF-8 (Comma delimited)" in Excel) and .xlsx work; nothing
needs to be installed.

Columns (first row = these names, in any order; see dataset/questions_template.csv):
    id           a whole number 1-99999, unique, and NEVER reused for another question
    type         MCQ | Output | Code
    topic        exactly as in the syllabus (Syllabus.cpp), e.g. Stacks
    subtopic     exactly as in the syllabus, e.g. Stack Operations
    level        Easy | Medium | Hard
    question     the question text (for Output it may be left empty)
    code         code shown with the question (required for Output; starter code for Code)
    option1-4    the four choices (MCQ only)
    answer       MCQ: 1-4 or A-D | Output: the exact output | Code: a reference solution
    explanation  why the answer is right (shown after answering)

What it checks: every rule C++ checks, plus things only a person would catch
early (typos in topic names, "All of the above" options that shuffling would
break, duplicate ids). Every Output program is compiled and run with g++; if
the real output differs from the answer column, that row is refused and the
real output is shown, so you can fix the sheet.

Rows with problems are skipped and listed with their spreadsheet row number;
the good rows are still written. The result goes to dataset/questions_dataset.txt
(one question per line, TAB-separated, same format as data/questions.txt).
"""

import concurrent.futures
import csv
import difflib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
import zipfile

import question_ai
import web_app

HERE = os.path.dirname(os.path.abspath(__file__))
DATASET_DIR = os.path.join(HERE, "dataset")
TEMPLATE_NAME = "questions_template.csv"
DEFAULT_OUTPUT = os.path.join(DATASET_DIR, "questions_dataset.txt")
DATASET_FIRST_ID = 500000          # same as QuestionBank.h: CSV id 7 is saved as 500007
MAX_CSV_ID = 99999

# Accepted column names (lower case, spaces/hyphens turned into "_").
COLUMNS = {
    "id": ["id", "question_id", "qid"],
    "type": ["type", "question_type"],
    "topic": ["topic"],
    "subtopic": ["subtopic", "sub_topic"],
    "level": ["level", "difficulty"],
    "question": ["question", "question_text", "text"],
    "code": ["code", "starter_code"],
    "option1": ["option1", "option_1", "opt1", "option_a"],
    "option2": ["option2", "option_2", "opt2", "option_b"],
    "option3": ["option3", "option_3", "opt3", "option_c"],
    "option4": ["option4", "option_4", "opt4", "option_d"],
    "answer": ["answer", "correct_answer"],
    "explanation": ["explanation"],
}
REQUIRED = ["id", "type", "topic", "subtopic", "level", "question", "answer", "explanation"]

TYPE_NAMES = {"mcq": "MCQ", "multiplechoice": "MCQ",
              "output": "Output", "predictoutput": "Output", "predicttheoutput": "Output",
              "code": "Code", "coding": "Code", "codequestion": "Code"}
LEVELS = ["Easy", "Medium", "Hard"]

# Options are shuffled when loaded, so an option that points at other options breaks.
POINTS_AT_OTHERS = re.compile(r"\b(all|none|both)\s+(of\s+)?(the\s+)?(above|these|options)\b"
                              r"|\bboth\s+\(?[a-d1-4]\)?\s+(and|&)\s+\(?[a-d1-4]\)?(?!\w)"
                              r"|\boptions?\s+\(?[a-d1-4]\)?(?!\w)", re.I)


class DatasetError(Exception):
    """A problem with the whole file (not one row): nothing is written."""


# ------------------------------------------------------------------ reading

def find_source(folder=DATASET_DIR):
    """The question sheet in the dataset folder, or None. Any name works
    (questions.csv, DSA Questions.xlsx, questions.csv.csv ...), except the template."""
    if not os.path.isdir(folder):
        return None
    preferred = os.path.join(folder, "questions.csv")
    if os.path.exists(preferred):
        return preferred
    sheets = [os.path.join(folder, n) for n in os.listdir(folder)
              if n.lower().endswith((".csv", ".xlsx")) and n.lower() != TEMPLATE_NAME
              and not n.startswith("~$")]                      # ~$name.xlsx = Excel's lock file
    return max(sheets, key=os.path.getmtime) if sheets else None


def explain_missing(folder=DATASET_DIR):
    """What to tell the user when there is no sheet."""
    found = sorted(os.listdir(folder)) if os.path.isdir(folder) else []
    return ("No question sheet found in this folder:\n    " + folder + "\n"
            "It contains: " + (", ".join(found) if found else "nothing") + "\n"
            "Put your Excel sheet there (Save As > \"CSV UTF-8\", any file name), with the column\n"
            "names of " + TEMPLATE_NAME + ". Or run:  python import_dataset.py \"path\\to\\your file.csv\"")

def read_xlsx(path):
    """Reads the first sheet of an .xlsx file with the standard library only
    (an .xlsx file is a zip of XML files), so nothing has to be installed."""
    ns = {"m": "http://schemas.openxmlformats.org/spreadsheetml/2006/main",
          "r": "http://schemas.openxmlformats.org/officeDocument/2006/relationships",
          "p": "http://schemas.openxmlformats.org/package/2006/relationships"}
    text_of = lambda node: "".join(t.text or "" for t in node.iter(f"{{{ns['m']}}}t"))
    try:
        with zipfile.ZipFile(path) as z:
            names = set(z.namelist())
            shared = []
            if "xl/sharedStrings.xml" in names:
                shared = [text_of(si) for si in ET.fromstring(z.read("xl/sharedStrings.xml")).findall("m:si", ns)]
            book = ET.fromstring(z.read("xl/workbook.xml"))
            first = book.find("m:sheets/m:sheet", ns).get(f"{{{ns['r']}}}id")
            rels = ET.fromstring(z.read("xl/_rels/workbook.xml.rels"))
            target = next(r.get("Target") for r in rels.findall("p:Relationship", ns) if r.get("Id") == first)
            target = target.lstrip("/")
            sheet = ET.fromstring(z.read(target if target.startswith("xl/") else "xl/" + target))
    except (zipfile.BadZipFile, KeyError, StopIteration, AttributeError, ET.ParseError):
        raise DatasetError("This .xlsx file could not be read. In Excel, use File > Save As > "
                           "\"CSV UTF-8 (Comma delimited)\" and put that file in the dataset folder instead.")
    table = []
    for row in sheet.iter(f"{{{ns['m']}}}row"):
        values = {}
        for c in row.findall("m:c", ns):
            letters = re.match(r"[A-Z]+", c.get("r", "A")).group()
            col = 0
            for ch in letters:
                col = col * 26 + ord(ch) - 64
            kind, v = c.get("t"), c.find("m:v", ns)
            if kind == "s" and v is not None:
                value = shared[int(v.text)]
            elif kind == "inlineStr":
                value = text_of(c)
            elif v is not None and v.text is not None:
                value = v.text
                if kind != "str" and re.fullmatch(r"-?\d+\.0+", value):
                    value = value.split(".")[0]                # Excel stores 7 as 7.0
            else:
                value = ""
            values[col - 1] = value
        if values:
            index = int(row.get("r", len(table) + 1)) - 1
            while len(table) < index:
                table.append([])                               # keep Excel's row numbers
            table.append([values.get(i, "") for i in range(max(values) + 1)])
    return table


def read_rows(path):
    """Returns (rows, notes): rows are dicts keyed by our column names, with 'row' = spreadsheet row number."""
    notes = []
    if path.lower().endswith(".xlsx"):
        table = read_xlsx(path)
    else:
        try:
            with open(path, newline="", encoding="utf-8-sig") as f:
                table = list(csv.reader(f))
        except UnicodeDecodeError:
            with open(path, newline="", encoding="cp1252") as f:
                table = list(csv.reader(f))
            notes.append("The CSV is not UTF-8, so symbols like ≤ or → may be wrong. "
                         "In Excel, save as \"CSV UTF-8 (Comma delimited)\".")
    skipped = 0
    while table and not any(str(v).strip() for v in table[0]):
        table.pop(0)                    # empty rows above the header
        skipped += 1
    if not table:
        raise DatasetError("The file is empty.")

    header = [re.sub(r"[\s\-]+", "_", h.strip().lower()) for h in table[0]]
    where = {}
    for name, aliases in COLUMNS.items():
        for i, h in enumerate(header):
            if h in aliases:
                where[name] = i
                break
    missing = [c for c in REQUIRED if c not in where]
    if missing:
        raise DatasetError("Missing column(s): " + ", ".join(missing) + ". The first row must name the columns "
                           "(see dataset/questions_template.csv). Found: " + ", ".join(table[0]))

    rows = []
    for n, values in enumerate(table[1:], start=2 + skipped):   # n = the row number Excel shows
        if not any(v.strip() for v in values):
            continue                    # blank line in the sheet
        row = {name: (values[i] if i < len(values) else "").replace("\r\n", "\n") for name, i in where.items()}
        for name in COLUMNS:
            row.setdefault(name, "")
        row["row"] = n
        rows.append(row)
    return rows, notes


def load_syllabus():
    """The syllabus comes from the C++ program itself, so topic names live in one place (Syllabus.cpp)."""
    if web_app.needs_build() and not web_app.build():
        raise DatasetError("The C++ program could not be built, so the syllabus can't be read.")
    out = subprocess.run([web_app.EXE_PATH, "--syllabus"], capture_output=True, text=True, timeout=30)
    return {t["name"]: t["subtopics"] for t in json.loads(out.stdout)}


# ------------------------------------------------------------------ checking one row

def _suggest(word, choices):
    close = difflib.get_close_matches(word, choices, n=1, cutoff=0.6)
    return f' Did you mean "{close[0]}"?' if close else ""


def _match(word, choices):
    """Exact name, or the same name with different capitals/spaces. None if not found."""
    key = " ".join(word.split()).lower()
    for c in choices:
        if c.lower() == key:
            return c
    return None


def check_row(row, syllabus):
    """Returns (record, problems). record is a dict ready to save, or None."""
    problems = []
    get = lambda k: row[k].strip()

    raw_id = get("id")
    if not re.fullmatch(r"\d+(\.0+)?", raw_id) or not 1 <= int(float(raw_id)) <= MAX_CSV_ID:
        problems.append(f'id must be a whole number from 1 to {MAX_CSV_ID} (found "{raw_id}").')
        qid = None
    else:
        qid = int(float(raw_id))

    qtype = TYPE_NAMES.get(re.sub(r"[\s\-_]", "", get("type").lower()))
    if qtype is None:
        problems.append(f'type must be MCQ, Output or Code (found "{get("type")}").')

    level = _match(get("level"), LEVELS)
    if level is None:
        problems.append(f'level must be Easy, Medium or Hard (found "{get("level")}").'
                        + _suggest(get("level").capitalize(), LEVELS))

    topic = _match(get("topic"), list(syllabus))
    subtopic = None
    if topic is None:
        problems.append(f'topic "{get("topic")}" is not in the syllabus.' + _suggest(get("topic"), list(syllabus)))
    else:
        subtopic = _match(get("subtopic"), syllabus[topic])
        if subtopic is None:
            problems.append(f'subtopic "{get("subtopic")}" is not a subtopic of {topic}.'
                            + _suggest(get("subtopic"), syllabus[topic]))

    text, code = get("question"), row["code"].rstrip()
    answer, explanation = row["answer"].strip("\n"), get("explanation")
    options = [get(f"option{k}") for k in range(1, 5)]
    if not explanation:
        problems.append("explanation is empty.")

    if qtype == "MCQ":
        if not text:
            problems.append("question is empty.")
        if any(o == "" for o in options):
            problems.append("an MCQ needs all four options (option1 to option4).")
        elif len({" ".join(o.lower().split()) for o in options}) < 4:
            problems.append("two options are the same.")
        for o in options:
            if POINTS_AT_OTHERS.search(o):
                problems.append(f'option "{o}" refers to other options. Options are shuffled for every '
                                "question, so rewrite it as a statement of its own.")
        a = answer.strip().upper().rstrip(".)")
        if a in ("A", "B", "C", "D"):
            answer = str("ABCD".index(a) + 1)
        elif re.fullmatch(r"[1-4](\.0+)?", a):
            answer = str(int(float(a)))
        elif _match(answer, options):
            answer = str(options.index(_match(answer, options)) + 1)
        else:
            problems.append(f'MCQ answer must be 1-4 or A-D (found "{answer.strip()}").')
    elif qtype == "Output":
        if not code.strip():
            problems.append("an Output question needs a program in the code column.")
        if not answer.strip():
            problems.append("answer (the expected output) is empty.")
        text = text or "What does this code print?"
        options = ["", "", "", ""]
    elif qtype == "Code":
        if not text:
            problems.append("question is empty.")
        if not answer.strip():
            problems.append("answer (a reference solution) is empty. The AI grader compares against it.")
        options = ["", "", "", ""]

    if problems:
        return None, problems
    record = {"id": qid, "type": qtype, "topic": topic, "subtopic": subtopic, "level": level,
              "text": text, "code": code, "options": options, "answer": answer, "explanation": explanation,
              "row": row["row"]}
    return record, []


# ------------------------------------------------------------------ g++ check of Output answers

def run_program(code, gpp):
    """Compiles and runs one program. Returns (status, detail): status is ok / compile / crash / blocked."""
    if question_ai.BLOCKED.search(code):
        return "blocked", ""
    with tempfile.TemporaryDirectory() as folder:
        src = os.path.join(folder, "q.cpp")
        exe = os.path.join(folder, "q.exe" if os.name == "nt" else "q")
        with open(src, "w", encoding="utf-8") as f:
            f.write(code)
        try:
            built = subprocess.run([gpp, "-std=c++11", "-w", "-o", exe, src],
                                   capture_output=True, text=True, timeout=60)
            if built.returncode != 0:
                first = next((l for l in built.stderr.splitlines() if "error" in l), built.stderr.strip()[:200])
                first = re.sub(r"^.*?q\.cpp:(\d+):\d+:\s*(error:\s*)?", r"line \1: ", first)
                return "compile", first
            ran = subprocess.run([exe], capture_output=True, text=True, timeout=5, cwd=folder,
                                 stdin=subprocess.DEVNULL)
        except subprocess.TimeoutExpired:
            return "crash", "it ran for more than 5 seconds"
        except OSError as e:
            return "crash", str(e)
        if ran.returncode != 0:
            return "crash", f"it exited with code {ran.returncode}"
        return "ok", ran.stdout[:2000]


def verify_outputs(records, gpp, progress=True):
    """Checks every Output record. Returns (records to keep, problems by row, warnings)."""
    outputs = [r for r in records if r["type"] == "Output"]
    keep_ids, problems, warnings = set(), {}, []
    if not outputs:
        return records, problems, warnings
    with concurrent.futures.ThreadPoolExecutor(max_workers=min(8, os.cpu_count() or 2)) as pool:
        results = list(pool.map(lambda r: run_program(r["code"], gpp), outputs))
    for r, (status, detail) in zip(outputs, results):
        if status == "ok" and question_ai._same_output(detail, r["answer"]):
            keep_ids.add(id(r))
        elif status == "ok":
            shown = detail.strip().replace("\n", "\\n") or "(nothing)"
            problems[r["row"]] = [f'the program prints "{shown}", but the answer column says '
                                  f'"{r["answer"].strip()}". Fix the answer (or the code).']
        elif status == "blocked":
            keep_ids.add(id(r))
            warnings.append(f"row {r['row']}: not checked with g++ (it uses files, processes or <cstdio>/<cstdlib>). "
                            "Check this answer by hand.")
        elif status == "compile":
            problems[r["row"]] = [f"the program doesn't compile: {detail}"]
        else:
            problems[r["row"]] = [f"the program doesn't finish normally: {detail}."]
    if progress:
        print(f"g++ checked {len(outputs)} predict-output programs.")
    return [r for r in records if r["type"] != "Output" or id(r) in keep_ids], problems, warnings


# ------------------------------------------------------------------ the whole file

def convert(rows, syllabus, verify=True, gpp=None, progress=True):
    """Returns (records, problems {row: [messages]}, warnings)."""
    records, problems, warnings = [], {}, []
    for row in rows:
        record, issues = check_row(row, syllabus)
        if issues:
            problems[row["row"]] = issues
        else:
            records.append(record)

    first_row = {}
    unique = []
    for r in records:                   # a reused id would mix up students' old answers
        if r["id"] in first_row:
            problems[r["row"]] = [f"id {r['id']} is already used on row {first_row[r['id']]}. "
                                  "Give every question its own id, and never reuse an old one."]
        else:
            first_row[r["id"]] = r["row"]
            unique.append(r)
    records = unique

    if verify:
        gpp = gpp or shutil.which("g++")
        if gpp is None:
            warnings.append("g++ was not found, so predict-output answers were NOT checked. "
                            "Install MinGW-w64 and run this again before the final submission.")
        else:
            records, out_problems, out_warnings = verify_outputs(records, gpp, progress)
            problems.update(out_problems)
            warnings += out_warnings
    else:
        warnings.append("--no-verify: predict-output answers were NOT checked with g++.")

    seen = {}
    for r in records:
        key = (r["type"], " ".join(r["text"].lower().split()), " ".join(r["code"].split()))
        if key in seen:
            warnings.append(f"row {r['row']}: same question as row {seen[key]} (both kept).")
        else:
            seen[key] = r["row"]
    return records, problems, warnings


def write_dataset(records, path):
    """Same format as data/questions.txt. Written to a temporary file first, then renamed."""
    lines = []
    for r in sorted(records, key=lambda r: r["id"]):
        fields = [DATASET_FIRST_ID + r["id"], r["type"], r["topic"], r["subtopic"], r["level"], 0,
                  r["text"], r["code"], *r["options"], r["answer"], r["explanation"]]
        lines.append("\t".join(web_app.escape_field(f) for f in fields))
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    temp = path + ".tmp"
    with open(temp, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    os.replace(temp, path)


def coverage(records, syllabus):
    """Text report: questions per topic, level and type, and the gaps students will notice."""
    out = [f"{'Topic':<22}{'Total':>6}{'Easy':>6}{'Med':>6}{'Hard':>6}{'MCQ':>6}{'Out':>6}{'Code':>6}"]
    gaps, thin = [], 0
    for topic, subs in syllabus.items():
        mine = [r for r in records if r["topic"] == topic]
        count = lambda k, v: sum(1 for r in mine if r[k] == v)
        out.append(f"{topic:<22}{len(mine):>6}" + "".join(f"{count('level', l):>6}" for l in LEVELS)
                   + "".join(f"{count('type', t):>6}" for t in ("MCQ", "Output", "Code")))
        for s in subs:
            if not any(r["subtopic"] == s for r in mine):
                gaps.append(f"{topic} > {s}")
        for l in LEVELS:
            for t in ("MCQ", "Output", "Code"):
                if sum(1 for r in mine if r["level"] == l and r["type"] == t) < 5:
                    thin += 1
    out.append(f"{'TOTAL':<22}{len(records):>6}")
    if gaps:
        out.append(f"\nSubtopics with no questions ({len(gaps)}): practice on these is written by the AI:")
        out += ["  " + g for g in gaps]
    combos = len(syllabus) * 9
    out.append(f"\n{thin} of {combos} topic+level+type choices have fewer than 5 questions. Longer sessions "
               "on those are filled up by the AI (or are shorter if the AI is not set up).")
    return "\n".join(out)


def main(argv):
    args = [a for a in argv if not a.startswith("--")]
    verify = "--no-verify" not in argv
    brief = "--brief" in argv
    source = args[0].strip('"') if args else find_source()
    if source is None:
        print(explain_missing())
        return 1
    if not os.path.exists(source):
        print(f"This file doesn't exist: {source}")
        return 1
    try:
        rows, notes = read_rows(source)
        syllabus = load_syllabus()
    except DatasetError as e:
        print("Nothing was written: " + str(e))
        return 1
    print(f"Read {len(rows)} rows from {os.path.relpath(source, HERE)}.")
    records, problems, warnings = convert(rows, syllabus, verify)
    for note in notes:
        print("NOTE: " + note)

    if problems:
        print(f"\n{len(problems)} row(s) SKIPPED (row numbers as in Excel):")
        for row in sorted(problems):
            for message in problems[row]:
                print(f"  row {row}: {message}")
    if warnings:
        print(f"\n{len(warnings)} warning(s):")
        for w in warnings:
            print("  " + w)
    if not records:
        print("\nNothing was written: no row passed the checks.")
        return 1

    write_dataset(records, DEFAULT_OUTPUT)
    print(f"\nSaved {len(records)} questions to {os.path.relpath(DEFAULT_OUTPUT, HERE)}.")
    if brief:
        print("(For the questions-per-topic table, run:  python import_dataset.py)")
    else:
        print("\n" + coverage(records, syllabus))
        print("\nRestart web_app.py to use the new question set.")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
