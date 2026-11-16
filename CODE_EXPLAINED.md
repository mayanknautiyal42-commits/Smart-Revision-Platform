# Code Explained (simple words)

## The big picture

```
main.cpp
  └── Platform  (the "brain": users, questions, saving, statistics, OTPs)
        ├── Syllabus          the 11 topics and their subtopics
        ├── QuestionBank      every saved AI question (Question* in a hash map)
        ├── RevisionManager   topic graph, BFS, heap ranking, level tips
        ├── users             hash map: email -> User* (Student or Admin)
        └── Storage           reads/writes the files in data/

Two front ends use the same Platform:
  ConsoleApp  -> terminal menus           (run: smart_revision)
  ApiServer   -> JSON for the website     (run: smart_revision --api, started by web_app.py)

Python around the C++ program:
  web_app.py      serves the page, forwards commands, runs the AI and email jobs
  import_dataset.py  turns the question spreadsheet (CSV) into dataset/questions_dataset.txt
  question_ai.py  prompts, checks on AI output, g++ check, code grading, mistake explanations
  ai_client.py    one HTTP request to Gemini / OpenAI / Anthropic
  mailer.py       sends the OTP email
  settings.py     reads config.json (API key, email login)
```

## The question set (Practice)

**Making it** (`import_dataset.py`; `web_app.py` runs it at start-up when the
sheet in `dataset/` is new or changed, by comparing file times):
1. `find_source()` picks the sheet (any name; not the template or Excel's
   `~$` lock file). `read_rows()` reads a CSV, or an .xlsx with
   `read_xlsx()`, which opens it as the zip of XML files it really is, so
   nothing needs to be installed. Column names may be
   written a few ways ("Sub Topic", "Difficulty", "Option A"...).
2. `load_syllabus()` runs `smart_revision --syllabus`, so the topic names come
   from `Syllabus.cpp` and are never written twice.
3. `check_row()` checks each row: id 1–99999, type, level, topic and subtopic
   (with "Did you mean ...?" for typos), 4 different MCQ options, no option
   like "All of the above" (options are shuffled, so it would point at the
   wrong options), answer 1–4 or A–D, explanation present.
4. `verify_outputs()` compiles and runs every predict-output program with g++
   (in parallel). If the program prints something else, the row is refused
   and the real output is shown.
5. `write_dataset()` writes the good rows in the same 14-field format as
   `data/questions.txt`, ids moved to 500000 + id, through a temporary file.

**Loading it** (`Platform::loadDataset()`): every line goes through the same
`checkQuestion()` as an AI question, then `QuestionBank::addDatasetRecord()`.
A bad line is skipped and reported on the terminal (on `cerr`, because `cout`
carries the JSON for `web_app.py`). AI ids still start at 1001: `add()` only
moves `nextId` for ids outside the question-set range.

**Using it** (`practice_start` → `Platform::pickPractice()`):
`QuestionBank::filterDataset()` finds the matches, `orderForStudent()` puts
them in this order, and the first `count` start the session:
1. shuffle (Fisher-Yates), then split into never answered / wrong last time /
   right last time;
2. "right" ones come out of a **min-heap keyed on the time of the last
   answer**, so the one answered longest ago is repeated first;
3. `takeTurns()` (round robin) spreads each group over subtopics, then the
   whole list takes turns by level and by type. For "Mixed", the type mix
   wins over "unseen first": a student who has done every output question
   still gets some in a Mixed session.
4. If the set had fewer than `count` questions, `pickPractice(..., fill)`
   adds similar questions of the same topic (never code questions). The AI
   never writes practice questions; it only writes revision questions.

## The Revision tab

`showRevision()` in `index.html` calls `plan` (C++ `cmdPlan`, plus `aiReady`
added by `web_app.py`). `RevisionManager::rankTopics()` pushes every
attempted topic into a **min-heap** (`MyPriorityQueue`) keyed on accuracy
and pops them, so the list comes out weakest first. For each topic,
`Platform::getWeakSubtopics()` (another heap) gives its 2 weakest
subtopics. Each topic card has a **Revision** button that sends `generate`
with mode `revision` and that topic; the AI then writes the questions as
described below. Without an API key the page says so and the buttons are
disabled.

**The rule:** Practice questions come only from the question set (the
team's dataset); Revision questions come only from the AI (API key). If
the AI is down, Revision says so instead of showing set questions.

## What happens when a student starts a revision session (website, AI)

1. The browser sends `generate` with: token, mode (revision), topic,
   subtopic, level, type, count. (Mode practice goes to `practice_start`
   instead, see above.)
2. `web_app.py` asks C++ to **`prepare`**: C++ checks every value (is the
   subtopic in this topic? is the count 1 to 20?) and returns the subtopics,
   the student's **weak subtopics** (min-heap), recent question titles (so the
   AI doesn't repeat them) and recent mistakes (for revision).
3. `question_ai.plan_slots()` decides each question's topic, subtopic, level
   and type. Example: Mixed + All levels + 6 questions gives 2 MCQ, 2 output,
   2 code, spread over Easy / Medium / Hard. In revision, 2 of every 3
   questions go to the weak subtopics.
4. The slots are grouped by type and sent to the AI (in parallel). The AI
   only fills in the text, options, answer and explanation.
5. Every item is checked in `_record()`. Output programs are compiled and run
   with g++ by `verify_output()`; a wrong AI answer means the question is
   dropped. Missing questions are asked for once more.
6. Each good question goes to C++ **`import_question`**: the topic is checked
   again, the MCQ options are shuffled, and the question is saved to
   `data/questions.txt`. C++ returns the new id.
7. C++ **`start_ids`** puts the ids in the session queue and returns the
   session's **serial number**. The browser then uses `next` and `answer`.

**Faster start:** if more than 4 questions were asked for, steps 4–6 run for
the first 2 slots only, and the session starts. A background thread
(`write_rest` in `web_app.py`) writes the other slots; each finished AI
request is checked, imported and added with **`append_ids`** (C++
`QuizSession::append()` pushes the ids onto the same queue). While it runs,
Python shows the expected total, so the page doesn't show "See results" too
early, and `next` waits if the student has answered everything so far. If the
student starts another session, the serial changes and late questions are
refused (`stale`), and starting a review cancels the job.

**AI busy:** `ai_client.complete_json()` retries HTTP 429/500/502/503/504 and
timeouts after 3 s and 8 s, then tries `fallback_model`. A wrong key (401) is
not retried.

## What happens when a student answers

- **MCQ / Predict output:** `answer` goes to C++. `Platform::submitAnswer()`
  calls `q->checkAnswer()` (polymorphism), updates the statistics, appends a
  line to `attempts.txt` (with the student's answer), and tells the session.
- **Code question:** `web_app.py` first asks C++ `current_info`. Because
  `q->needsAiGrading()` is true, Python sends the problem, the hidden reference
  solution and the student's code to the AI (`grade_code`). The verdict and
  feedback go back to C++ with `answer_graded`, which records them exactly
  like any other answer.

The browser never receives an answer key: `toJson()` leaves it out for all
three question types.

## My mistakes and "Explain my mistake"

A question is a mistake if the student's **last** answer to it was wrong
(`Student::lastResult`). `Student::lastAnswer` remembers what they answered.
"Explain my mistake" sends the question, the student's answer, the right
answer and the explanation to the AI, which writes 4 parts: why the answer is
wrong, the idea to remember, how to avoid it, and a quick check question. The
note is saved in `data/notes.txt` and reused, so the AI is asked only once per
mistake.

## Forgot password (OTP)

1. `forgot_start` (Python): makes a 6-digit code with `secrets` (a secure
   random generator) and calls C++ `otp_start`.
2. `Platform::startPasswordReset()`: the account must exist with that role;
   at most one code per minute. Only a **hash** of the code is stored in the
   `otps` hash map, with an expiry time 10 minutes ahead and 5 tries.
3. Python emails the code (`mailer.py`). If email isn't set up, it prints the
   code in the terminal instead.
4. `reset_password` → `Platform::finishPasswordReset()`: checks expiry, tries
   and the hash, checks the new password, saves **all** users again with
   `Storage::writeLines()` (written to a temporary file first, then renamed),
   and `ApiServer::endSessionsOf()` closes every open login of that account.

## Data structures (header files)

**MyStack.h**: `MyVector` (array that doubles when full), `insertionSort()`,
and `MyStack` (LIFO, used for recent answers). Rule of Three in both.

**MyQueue.h**: FIFO linked list. Session question order and BFS.

**MyHashMap.h**: separate chaining, rehashing above 75% load. Used for users
by email, questions by id, every statistic, login tokens and OTPs.

**MyHashSet.h**, **MyPair.h**, **MyPriorityQueue.h** (binary min-heap).

## Users

**User** (abstract): email, full name, password hash, created time.
`hashPassword(email, password)` uses the email as the salt. `setPassword()`
is used by the password reset.

**Student**: hash maps of `MyPair(correct, attempts)` per topic, subtopic,
level, type, topic+level, and **day** (for the progress graph); `lastResult`,
`lastAnswer`, `lastFeedback` per question; a stack of recent attempts.

**Storage**: read lines, append a line, rewrite a whole file safely, split at
tabs, and `escape()` / `unescape()`: code contains tabs and new lines, so
they are stored as `\t` and `\n` to keep one record per line. The same rule is
used between Python and C++.

## Questions

**Question** (abstract): id, topic, subtopic, difficulty, explanation.

| Class | Type name | Answer checked by |
|---|---|---|
| `MCQQuestion` | MCQ | C++: option number |
| `OutputQuestion` | Output (predict the output) | C++: same text, extra spaces ignored |
| `CodingQuestion` | Code (write code) | the AI (`needsAiGrading()` is true) |

**QuestionBank**: loads and saves `questions.txt` (14 fields per line, see the
comment in `QuestionBank.h`). `addRecord()` gives the next id, shuffles MCQ
options and appends the line. `shuffleOptions()` is Fisher-Yates; each swap
uses `mixBits()`, a small integer hash of (id, step). A plain linear
congruential generator seeded with consecutive ids (1001, 1002, ...) put the
right answer in some positions far more often, which students could learn.

**Syllabus**: topic → list of subtopics, plus `buildTopicGraph()`.

## Logic

**QuizSession**: one session; a queue of question ids.

**RevisionManager**: topic graph (adjacency list), `bfs()`, `rankTopics()`
(min-heap), `recommendNext()`, `levelSuggestion()`.

**Platform**: accounts (`registerStudent`, `createAdmin`, `login(role, ...)`),
OTP reset, `importQuestion`, `submitAnswer` / `submitGraded`,
`getMistakeIds`, `getWeakSubtopics` (min-heap), `getRecentQuestionIds`,
notes, class statistics. `init()` loads the syllabus, questions, users,
answers (replayed to rebuild statistics) and notes.

## The website (web/index.html)

- Log in: two tabs, **Student** (log in / sign up) and **Admin** (log in),
  both with **Forgot password** → email → OTP + new password.
- Practice: topic, subtopic, level, type, 1–20 slider, "Start practice" (question set).
- Quiz: MCQ buttons, output text box, or a code editor (Tab indents).
- Results: "more MCQs" or "now try code questions".
- My mistakes, My report, Revision plan, Admin pages.
- Graphs are drawn by our own code (CSS bars and SVG), no chart library:
  `accuracyChart()`, `rightWrongChart()`, `progressChart()`, `topicMap()`.
- `esc()` escapes every piece of text before it goes into the page, including
  AI text, so nothing the AI writes can run as HTML.

## Practice questions for the viva

1. Why are `Question` and `User` abstract? What does `needsAiGrading()` change?
2. Walk through a revision request from the browser to the saved file. How is a practice request different?
3. Why does Python plan the slots instead of letting the AI choose topics?
4. How are wrong AI answers caught for predict-output questions? Why can't the same be done for MCQs?
5. How is the OTP stored? Why a hash, an expiry, a try limit and a resend limit?
6. Why does the forgot-password form give the same message for unknown emails?
7. Why is the users file rewritten through a temporary file?
8. What was wrong with seeding the shuffle with consecutive ids, and how does `mixBits()` fix it?
9. How does the server stop a student from opening admin pages or calling `otp_start`?
10. Name two ways a student could get a wrong grade on a code question.
11. Why does the session start after 3 questions? What stops a late question
    from joining the wrong session?
12. Why is a lower "thinking" level faster, and what does it cost?
13. Why do question-set ids start at 500001? What would break if an id were reused?
14. Why are question-set MCQ options shuffled with the id as the seed, not a random seed?
15. Practice orders questions by unseen → wrong → right. Where is the min-heap used, and why a heap?
16. Why does the importer ask C++ for the syllabus instead of having its own list?
17. On the Revision tab, why is a min-heap a good fit for "weakest first"? What would a sorted array cost?
18. Why are code questions never used to fill up a practice session?
19. Why do Practice and Revision use different question sources? What does each source do better?
