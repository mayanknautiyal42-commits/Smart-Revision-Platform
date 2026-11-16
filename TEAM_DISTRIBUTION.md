# Team Distribution (AI version)

Each member keeps their earlier area and takes the new files closest to it.
This is a suggestion: change it if your team prefers another split, but make
sure every member can explain their own files in the review.

## File-wise distribution

| Member | Area | Files |
|---|---|---|
| **Abhijeet Rawat** | Linear data structures, heap, revision algorithms, practice order, terminal app, testing | `MyStack.h`, `MyQueue.h`, `MyPriorityQueue.h`, `RevisionManager.h/.cpp`, `Platform::orderForStudent()` / `takeTurns()`, `ConsoleApp.h/.cpp`, `test_mylib.cpp`, `test_core.cpp`, `test_web.py` |
| **Mayank Nautiyal** | Hashing, users, email login, OTP reset, saving | `MyHashMap.h`, `MyHashSet.h`, `MyPair.h`, `User.h/.cpp`, `Admin.h`, `Student.h/.cpp`, `Storage.h/.cpp`, `InputUtils.h`, `Platform.h/.cpp`, `mailer.py` |
| **Aananda Sharma** | Question classes (OOP) and the website | `Question.h/.cpp`, `MCQQuestion.h/.cpp`, `OutputQuestion.h/.cpp`, `CodingQuestion.h/.cpp`, `web/index.html` (screens and graphs) |
| **Milan Negi** | Question bank, question set, syllabus, AI, web back end | `QuizSession.h/.cpp`, `QuestionBank.h/.cpp`, `Syllabus.h/.cpp`, `ApiServer.h/.cpp`, `Json.h`, `web_app.py`, `import_dataset.py`, `question_ai.py`, `ai_client.py`, `settings.py`, `main.cpp`, `Makefile`, `.bat` files |

## What each member explains in the final review

- **Abhijeet:** growing arrays, stack and queue, min-heap (now also used for
  weak subtopics and for practice repeats), how practice orders questions
  (unseen → wrong → right, round robin by level and type), BFS and the recommendation, the terminal menus, and what
  the three test files check (including the fake AI in `test_web.py`).
- **Mayank:** hashing and rehashing; `User` / `Student` / `Admin`; email
  checks; password hashing; the whole OTP flow (hash, expiry, tries, resend
  limit, ending old logins); how files are saved, rewritten safely and
  replayed at start-up; sending email with SMTP.
- **Aananda:** abstract `Question` and the three question types,
  `needsAiGrading()`, why `toJson()` hides the answer; the login tabs, quiz
  screens, My mistakes page, and how the report graphs are drawn.
- **Milan:** the question set from spreadsheet to session (importer checks,
  g++ check, id range, `loadDataset`, `practice_start`); a revision session
  from the AI to the saved file (prepare → slots → AI → checks → g++ → import → start); the prompts; how code is
  graded; the option shuffle fix; how `web_app.py` keeps internal commands
  away from the browser and keeps the API key off the page.

## Remaining work for the whole team

0. **Grow the question set.** A bank of 318 checked questions is
   included (`dataset/dsa_question_bank.csv`, ids 3001-3318); read its MCQ
   keys once. New rows use ids from 4000 upward. To reach 500+, split the topics between members;
   each writes the questions for their topics in the shared sheet and runs
   `python import_dataset.py` until no rows are skipped. Then a *different*
   member checks every MCQ key in that topic: the importer can verify
   predict-output answers, but not MCQ keys. Aim for at least 5 questions in
   every topic/level/type choice (the importer's coverage table shows the gaps).
1. **Check AI question quality** (revision sessions). Generate about 10 questions per topic and
   have each member check the ones in their area. Count how many MCQs had a
   wrong key or an unclear question, per topic. Report the number honestly;
   it is the main risk of this project.
2. **Check AI code grading.** Write 10 answers you know are right and 10 you
   know are wrong (off-by-one, missing edge case, wrong order). See how many
   the AI grades correctly.
3. **User testing.** 5 to 10 classmates take a short test, use the platform
   for a fixed time, then take a similar test. Compare scores and collect
   feedback. Report what didn't improve too.
4. **Final report and presentation.** Each member writes and presents their
   own area, including its limitations.
