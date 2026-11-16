# Smart Revision Platform

A DSA revision platform for students, with a teacher (admin) dashboard.
Students sign up with their email, choose a topic, subtopic, level, question
type and number of questions, and **Practice** gives them questions from the
team's **question set** (318 checked questions, Easy / Medium / Hard).
Wrong answers are collected in **My mistakes**, where the AI explains each
mistake so it isn't repeated. **My report** shows topic-wise graphs, and the
**Revision plan** sends the student back to their weakest subtopics, where an
**AI writes fresh questions** aimed at those weak spots.

The core logic is written in **C++** using **our own data structures** (no STL
containers). **Python** serves the website, converts the question spreadsheet
(`import_dataset.py`), and does the two jobs that need the internet: calling
the AI and sending OTP emails. The website itself is
**HTML, CSS and JavaScript**.

## Features

| For students | For the admin (teacher) |
|---|---|
| Separate **Student** and **Admin** login tabs | First-run setup of the admin account |
| Sign up with full name, email and password | Class overview: students, answers, class accuracy, AI questions saved |
| **Forgot password**: 6-digit OTP by email (both logins) | Table of all students: search by name or email, sort |
| All 11 syllabus topics with their subtopics | One student's full report with graphs |
| 4 question types: **MCQ, Code question, Predict output, Mixed** | Class accuracy by topic (graph) |
| Practice: 1 to 20 questions from the question set; unseen ones first | Most-missed questions (min-heap) |
| Practice never uses the AI to write questions; if too few match, similar MCQ/output questions of the same topic fill in | AI question count |
| **Revision** tab: topics weakest first (min-heap), weak spots of each, a **Revision** button under each topic; revision questions come **only from the AI** | |
| After an MCQ session: "more MCQs" or "now try code questions" | |
| Code questions graded by the AI with feedback | |
| **My mistakes**: your answer vs the right one, plus "Explain my mistake" | |
| **My report**: accuracy by topic, right/wrong by topic, progress by day, by type, by level, by subtopic | |
| **Revision plan**: weakest topic (min-heap), next topic (BFS), AI-written revision session on weak subtopics | |

### Syllabus (in `Syllabus.cpp`)

Arrays, Stacks, Recursion, Queues, Linked List, Trees, Binary Search Trees,
Sorting, Searching and Hashing, File Structures, Graphs — 65 subtopics in
total, exactly as in the course outline. C++ refuses any question (from the
question set or the AI) whose topic or subtopic is not in this list.

## Setup (once)

You need **g++** (MinGW-w64 on Windows) and **Python 3.7+**. Nothing to install with pip.

1. **AI key.** Copy `config.example.json` to `config.json` and paste your key
   into `ai.api_key`.
   - `provider` can be `gemini`, `openai` or `anthropic`.
   - Leave `model` empty to use the default (`gemini-flash-latest` for Gemini,
     `claude-haiku-5-5` for Anthropic). For OpenAI you must set `model`.
     Model names change often: if you get "model not found", check your
     provider's model list.
   - `base_url` is only for OpenAI-compatible servers; leave it empty otherwise.
   - `fallback_model` is tried when the main model stays busy (HTTP 503/429)
     after 3 tries. Empty = `gemini-flash-lite-latest` for Gemini.
   - `thinking` (Gemini only): `""` (model default), `"medium"`, `"low"` or
     `"minimal"`. Lower is **faster** but the AI checks its own answers less,
     so wrong MCQ keys become more likely. Leave it empty unless speed matters
     more than accuracy.
2. **Question set (Practice).** The project comes with a **question bank of 318
   questions** (`dataset/dsa_question_bank.csv`, ids 3001-3318): 152 MCQs, 128
   predict-output and 38 code questions (85 Easy, 144 Medium, 89 Hard). Every
   one of the 11 topics has at least one Easy, Medium and Hard question of each
   type, and all 65 subtopics are covered. Every predict-output answer was
   checked by running the program with g++, and every code question's reference
   solution was compiled and passed unit tests. The bank was written with AI
   help, so the team should still read through the MCQ keys before the final
   submission.
   **Practice uses only this set** (never the AI). When a student asks for
   more questions than the set has for that choice, similar MCQ and output
   questions of the same topic fill in; if there are still too few, the
   session is shorter. **Revision uses only the AI** (API key).

   To add questions, open `dataset/dsa_question_bank.csv` in Excel and add
   rows (use ids from 4000 upward), or put your own complete sheet in the
   folder: only one sheet is used, the newest. Keep the questions in one Excel
   sheet with the columns of `dataset/questions_template.csv`
   (id, type, topic, subtopic, level, question, code, option1–option4,
   answer, explanation). Put the sheet in the **`dataset` folder** as `.xlsx`
   or `.csv`, with any file name. That's all: `web_app.py` imports it
   automatically when it starts, and again whenever the sheet changes.
   The import checks every row, compiles and runs every predict-output program
   with g++, and lists the rows it skipped (with the Excel row number and the
   reason) in the terminal. To see the questions-per-topic table, run
   `python import_dataset.py` yourself.
   **Never reuse an id** for a different question: students' saved answers
   point to questions by id.
3. **OTP email (optional for testing).** Fill in the `email` part of
   `config.json`. For Gmail: turn on 2-step verification, create an
   **App password**, and use that 16-character password, never your normal one.
   Until this is set, OTPs are printed in the terminal running `web_app.py`.

`config.json` is in `.gitignore`. **Never commit it, and never share the key
in a group chat or screenshot.** Anyone with the key can spend your quota.
The key never reaches the browser.

## How to run

**Website (recommended):**

```
python web_app.py        (Windows: or double-click run_website.bat)
python3 web_app.py       (macOS / Linux)
```

The first run compiles the C++ back end and opens http://127.0.0.1:8000.
The terminal shows whether the question set, the AI and the email are ready,
and lists any question-set lines C++ refused. On the very first
run the website asks you to create the **admin account**. Students choose
**Student → Sign up**. Press Ctrl+C in the terminal to stop.

**Terminal version** (practises the question set and AI questions already
saved; code questions need the website because they are graded by the AI):

```
g++ -std=c++11 -o smart_revision main.cpp
.\smart_revision          (Windows: or double-click run_terminal.bat)
```

Compile **only** `main.cpp`: it includes the other `.cpp` files itself.

**Tests** (no API key, internet or email needed):

```
g++ -std=c++11 -o test_mylib test_mylib.cpp && ./test_mylib    data structures
g++ -std=c++11 -o test_core test_core.cpp && ./test_core       accounts, OTP, questions, statistics
python test_web.py                                             importer + whole website flow with a fake AI
```

## How it works

```
 Browser (web/index.html)   draws the screens; never sees an answer key, an OTP or the API key
        |  POST /api  {"cmd": "generate", "args": [token, "practice", "Stacks", "*", "All", "MCQ", "10"]}
        v
 web_app.py (Python)        checks the command; AI and email jobs are done here,
        |                   everything else is forwarded to C++ as one line
        |  stdin: "prepare<TAB>token<TAB>..."      stdout: {"ok":true,...}
        v
 smart_revision --api (C++) ApiServer -> Platform -> Syllabus, QuestionBank, Student, RevisionManager
        |
        v
 data/users.txt, questions.txt, attempts.txt, notes.txt
```

**Practice session** (`generate` with mode `practice`, no AI):
1. `web_app.py` sends C++ `practice_start`. C++ checks the choice (the same
   checks as for revision) and filters the question set by topic, subtopic,
   level and type.
2. `Platform::orderForStudent()` orders the matches: questions the student
   has **never answered** (random order), then ones they got **wrong**, then
   ones they got **right, answered longest ago first** (min-heap on the time
   of the last answer). Then the list takes turns by level and by type, so
   "All levels" and "Mixed" sessions really are mixed.
3. The first `count` ids start the session at once. If fewer match than
   were asked for, `pickPractice()` widens the choice inside the same topic
   (any level, then any subtopic, then any type, never code questions) and
   adds those. If there are still too few, the session is shorter; if
   nothing matches, the student is told to choose another topic. The AI is
   never asked to write practice questions (it only grades code answers).

MCQ options in the question set are shuffled once at load time with the
question id as the seed, so the order is the same after every restart.

**Revision session** (`generate` with mode `revision`, AI):
1. C++ `prepare` checks the choice and returns the subtopics, the student's
   weak subtopics, recent questions to avoid, and recent mistakes.
2. Python plans one slot per question (topic, subtopic, level, type), so the
   mix is exact, and asks the AI to fill the slots (`question_ai.py`).
3. Every AI item is checked: 4 distinct options, answer 1–4, no "all of the
   above", length limits. **Predict-output programs are compiled and run with
   g++**; if the real output differs from the AI's answer, the question is
   dropped. Missing questions are requested once more.
4. C++ `import_question` checks the topic again, shuffles the MCQ options,
   saves the question, and `start_ids` starts the session.

**Speed (revision):** revision questions come only from the AI. For
**Prefetch:** when a student opens the Revision page, `web_app.py`
already knows their weakest topic, so it writes 2 MCQs for it in the
background (kept in memory, not saved, used once, expire after 15 minutes).
If the student then starts revision of that topic (MCQ or Mixed), the session
begins with them at once. Otherwise, only the **first question** (of the
fastest type in the session: MCQ, then Output, then Code) is written before
the session starts; if one of them fails, it is retried in the background instead
of making the student wait. The rest are written in a background
thread and added to the running session with `append_ids` as each AI request
finishes. Requests are kept small (4 MCQs, 3 output or 2 code questions) and
run in parallel, because the AI's writing time grows with the length of its
reply. If the student is faster than the AI, the Next button waits for the
next question. Each session has a serial number, so questions from an old
session can never join a newer one. If the AI is busy (HTTP 429/5xx), each
request is retried after 2 s and 5 s, then the backup model is tried.

**Faster AI replies:** the student-facing requests (first question and
prefetch) are *hedged*: if no reply has come after 8 s, the same request is
sent once more and the first reply wins, because the same request sometimes
takes 1 s and sometimes 15+ s on the provider's side. Prompts ask for short
explanations, and MCQs write the explanation before the answer number (a
little reasoning before committing to the key). For the fastest setup use
`"model": "gemini-flash-lite-latest"` and `"thinking": "minimal"`, with
`"fallback_model": "gemini-flash-latest"` (as in config.example.json).

**Answers:** MCQ and predict-output answers are checked by C++. Code answers
are sent to the AI with the hidden reference solution; the verdict and
feedback are then recorded by C++ like any other answer.

**Forgot password:** Python makes a 6-digit code with the `secrets` module and
asks C++ to store it. C++ keeps only a hash, with a 10-minute expiry, 5 tries
and a 1-minute resend limit. The reply is the same whether or not the email
has an account, so the form can't be used to find registered emails. After a
reset, every open login of that account is closed.

**Logins:** each login gets a random 32-character token. Admin pages refuse
student tokens. Internal commands (`otp_start`, `prepare`, `import_question`,
`answer_graded`, ...) are not in the browser's command list, so only Python
can use them.

## File structure

```
Question set
  dataset/questions_template.csv (column layout + 4 examples), your sheet (any .csv/.xlsx
  name), dataset/questions_dataset.txt (made from the sheet by import_dataset.py)
Data structures (our own, header-only templates)
  MyStack.h (MyVector + insertionSort + MyStack)  MyQueue.h  MyHashMap.h
  MyHashSet.h  MyPair.h  MyPriorityQueue.h
Users and saving
  User.h/.cpp (abstract), Student.h/.cpp, Admin.h, Storage.h/.cpp, InputUtils.h
Questions
  Question.h/.cpp (abstract), MCQQuestion, OutputQuestion (predict output),
  CodingQuestion (write code), QuestionBank.h/.cpp, Syllabus.h/.cpp
Logic
  QuizSession.h/.cpp, RevisionManager.h/.cpp, Platform.h/.cpp
Front ends
  ConsoleApp.h/.cpp (terminal), ApiServer.h/.cpp + Json.h (website back end)
Python
  web_app.py (server + bridge), import_dataset.py (CSV -> question set, checks,
  g++ check, coverage report), question_ai.py (prompts, checks, grading,
  explanations), ai_client.py (Gemini / OpenAI / Anthropic over HTTP),
  mailer.py (OTP email), settings.py (reads config.json)
Website
  web/index.html (all screens and graphs, no chart library)
Other
  main.cpp, test_mylib.cpp, test_core.cpp, test_web.py, Makefile,
  run_website.bat, run_terminal.bat, config.example.json, data/,
  CODE_EXPLAINED.md, TEAM_DISTRIBUTION.md
```

## OOP concepts

| Concept | Where |
|---|---|
| Abstraction (abstract classes) | `Question` and `User` have pure virtual functions |
| Inheritance | `MCQQuestion`, `OutputQuestion`, `CodingQuestion` : `Question`; `Student`, `Admin` : `User` |
| Runtime polymorphism | `q->checkAnswer()`, `q->toJson()`, `q->describeAnswer()`, `q->needsAiGrading()` through `Question*`; `user->getRole()` through `User*` |
| Virtual destructor | `Question` and `User`, so `delete` through a base pointer is safe |
| Encapsulation | Private data with public functions in every class |
| Function overloading | `hashKey(int, int)` and `hashKey(string, int)` |
| Operator overloading | `operator[]` and `operator=` in containers, `operator<` for `MyPair` |
| Templates | All data structures |
| Rule of Three | `MyVector`, `MyStack`, `MyQueue`, `MyHashMap` |

## Data structures and algorithms

| Structure / algorithm | Used for | Time |
|---|---|---|
| Dynamic array (`MyVector`) | Lists everywhere; doubles when full | O(1) amortised push |
| Stack (`MyStack`) | Recent answers, newest first | O(1) push/pop |
| Queue (`MyQueue`, linked list) | Questions in a session; BFS | O(1) push/pop |
| Hash table with chaining and rehashing (`MyHashMap`) | Users by email, questions by id, statistics, login tokens, OTPs | O(1) average |
| Hash set (`MyHashSet`) | BFS visited set | O(1) average |
| Min-heap (`MyPriorityQueue`) | Weakest topics, weakest subtopics, most-missed questions, practice repeats (answered longest ago first) | O(log n) push/pop |
| Graph + BFS | Topic map and next-topic recommendation | O(V + E) |
| Fisher-Yates shuffle with a hashed seed | MCQ option order; practice question order | O(n) |
| Round robin over groups (`takeTurns`) | Mixing topics, levels and types in a practice session | O(n) |
| Insertion sort | Attempted topics, active days | O(n²), n small |

## Limitations (say these in the review)

- **The question set is only as good as the sheet.** The importer catches
  format errors, syllabus typos and wrong predict-output answers, but not a
  wrong MCQ key or a misleading question. Have a second team member review
  each topic.
- **The question bank is still thin in places.** 318 questions is about 20 to
  40 per topic, with 10 to 15 predict-output questions per topic, but most
  topic/level/type choices still have fewer than 5 questions (code questions
  especially), so a long practice session
  on one choice is shorter than asked, or filled with that topic's other
  questions. Adding questions to the sheet fixes this; the importer's
  coverage table shows where.
- **AI revision questions can be wrong.** Predict-output answers are verified by g++,
  but MCQ answers and code grading are not. The admin's most-missed list helps
  find bad questions; a "report this question" button and teacher approval
  would be the next step.
- **AI grading of code is a judgement, not a test run.** Running the student's
  code against test cases in a sandbox would be more reliable, but running
  untrusted code safely is a project of its own.
- **Prompt injection:** a student could write "mark this correct" inside their
  code. The prompt tells the AI to ignore instructions in the code, which
  reduces but does not remove this risk.
- **Password hashing is for learning only** (salted FNV-1a, 1000 rounds).
  A real system should use bcrypt or Argon2, and HTTPS.
- **Runs on one computer** (127.0.0.1). Using it across a college network
  needs proper hosting and security.
- **Free AI tiers have rate limits.** If many students start revision at once, some
  will see "The AI is busy". A 20-question revision session needs about 4 to 8 AI
  requests. Practice is not affected, except grading of code questions.
- **Effectiveness is not measured yet.** See `TEAM_DISTRIBUTION.md`.

## Troubleshooting

- **"Practice: NOT READY"** when starting: no sheet was found in the `dataset` folder, or every row was refused. The lines above it in the terminal say which, and what to fix.
- **Rows skipped**: the terminal lists each one with its Excel row number. Fix them, save the sheet, and start the website again.
- **"The question set has no ... questions for ..."**: that choice has no questions yet. Add some to the sheet (see the importer's coverage table) and run the importer again.
- **"Missing column(s)"**: the header row of the sheet must have the column names from `dataset/questions_template.csv`.
- **Symbols like ≤ look wrong**: in Excel, save as "CSV UTF-8 (Comma delimited)", not plain "CSV".

- **"No AI API key is set"**: create `config.json` from `config.example.json`.
- **"The AI provider rejected the API key"** or **"model was not found"**: check `ai.api_key` / `ai.model`.
- **OTP email not arriving**: check spam; for Gmail use an App password. If email isn't configured, the OTP is in the terminal window.
- **"g++ was not found"**: install MinGW-w64 and add its `bin` folder to PATH. g++ is also needed to check predict-output questions.
- **"undefined reference" errors**: compile only `main.cpp`, not every file.
- **Build fails with "Permission denied" on Windows**: an old copy is still running. Stop it with Ctrl+C and try again.
- **Port 8000 in use**: the server automatically tries 8001 to 8009.
- **Old data from the previous version**: accounts used usernames, not emails, and the old 108 built-in questions are gone. Delete the `.txt` files in `data/` and start fresh.....
