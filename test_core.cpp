#include <iostream>
#include <string>
#include <cstdio>
#include "Storage.cpp"
#include "User.cpp"
#include "Student.cpp"
#include "Question.cpp"
#include "MCQQuestion.cpp"
#include "OutputQuestion.cpp"
#include "CodingQuestion.cpp"
#include "QuestionBank.cpp"
#include "Syllabus.cpp"
#include "QuizSession.cpp"
#include "RevisionManager.cpp"
#include "Platform.cpp"
using namespace std;

int failures = 0;

void check(bool condition, string name) {
    if (condition) {
        cout << "[PASS] " << name << "\n";
    } else {
        cout << "[FAIL] " << name << "\n";
        failures++;
    }
}

MyVector<string> mcqFields(string topic, string subtopic, string level, string text, int answer) {
    string raw = "MCQ\t" + topic + "\t" + subtopic + "\t" + level + "\t" + text + "\t\tA\tB\tC\tD\t" +
                 to_string(answer) + "\tbecause";
    return Storage::split(raw, '\t');
}

void clearFolder(string folder) {
    remove((folder + "/users.txt").c_str());
    remove((folder + "/attempts.txt").c_str());
    remove((folder + "/questions.txt").c_str());
    remove((folder + "/notes.txt").c_str());
    remove((folder + "/dataset.txt").c_str());
}

// One line of the question-set file; csvId is the id written in the CSV.
string datasetLine(int csvId, string type, string topic, string subtopic, string level, string answer,
                   string code = "") {
    MyVector<string> f;
    f.push_back(to_string(DATASET_FIRST_ID + csvId));
    f.push_back(type); f.push_back(topic); f.push_back(subtopic); f.push_back(level); f.push_back("0");
    f.push_back("Question " + to_string(csvId)); f.push_back(code);
    string opts[4] = {"right", "w1", "w2", "w3"};
    for (int k = 0; k < 4; k++) f.push_back(type == "MCQ" ? opts[k] : "");
    f.push_back(answer); f.push_back("because");
    string line = "";
    for (int k = 0; k < f.size(); k++) line += (k ? "\t" : "") + Storage::escape(f[k]);
    return line;
}

int main() {
    // ---------- syllabus
    Syllabus syllabus;
    loadSyllabus(syllabus);
    check(syllabus.getTopics().size() == 11, "Syllabus has 11 topics");
    check(syllabus.hasSubtopic("Graphs", "Floyd-Warshall Algorithm") &&
          syllabus.hasSubtopic("Arrays", "Sparse Matrices") &&
          !syllabus.hasSubtopic("Arrays", "Floyd-Warshall Algorithm"), "Subtopics belong to the right topic");

    // ---------- storage escaping
    string tricky = "line 1\n\tindented \\n not a newline";
    check(Storage::unescape(Storage::escape(tricky)) == tricky, "Escape and unescape round-trip");
    check(Storage::escape(tricky).find('\t') == string::npos && Storage::escape(tricky).find('\n') == string::npos,
          "Escaped text has no tabs or new lines");

    // ---------- email and password rules
    check(Platform::isValidEmail("ridam.sharma+dsa@gehu.ac.in"), "Valid email accepted");
    check(!Platform::isValidEmail("ridam") && !Platform::isValidEmail("a@b") && !Platform::isValidEmail("a b@c.com") &&
          !Platform::isValidEmail("a@@c.com") && !Platform::isValidEmail("a@c..com"), "Invalid emails refused");
    check(Platform::normalizeEmail("  Ridam@Gmail.COM ") == "ridam@gmail.com", "Emails are trimmed and lower-cased");
    string err;
    check(!Platform::isValidPassword("12345", err) && Platform::isValidPassword("123456", err), "Password needs 6+ characters");

    // ---------- hashing
    string h1 = User::hashPassword("ridam@gmail.com", "pass1234");
    check(h1 == User::hashPassword("ridam@gmail.com", "pass1234"), "Password hash is repeatable");
    check(h1 != User::hashPassword("milan@gmail.com", "pass1234"), "Same password, different email, different hash");
    check(h1.length() == 16 && h1.find("pass") == string::npos, "Hash does not contain the password");

    // ---------- question bank
    clearFolder("test_data");
    Storage::makeFolder("test_data");
    QuestionBank bank;
    bank.setFile("test_data/questions.txt");
    int positions[5] = {0, 0, 0, 0, 0};
    for (int i = 0; i < 40; i++) {
        int id = bank.addRecord(mcqFields("Arrays", "Address Calculation", "Easy", "Q" + to_string(i), 1), 100, err);
        Question* q = bank.get(id);
        positions[q->getCorrectAnswerText()[0] - '0']++;
        if (q->getCorrectAnswerText().substr(3) != "A") {
            positions[0]++;   // the right option text must stay "A" after shuffling
        }
    }
    check(positions[0] == 0, "Shuffling keeps the right option text");
    check(positions[1] >= 5 && positions[2] >= 5 && positions[3] >= 5 && positions[4] >= 5,
          "Shuffled options: consecutive ids spread the right answer over all 4 positions");

    // built field by field: the code itself contains a TAB
    MyVector<string> out = Storage::split("Output|Stacks|Stack Operations|Medium||CODE|||||1 2|trace", '|');
    out[5] = "int main() {\n\tcout << 1 << \" \" << 2;\n}";
    int outId = bank.addRecord(out, 100, err);
    Question* oq = bank.get(outId);
    check(oq != nullptr && oq->getTypeName() == "Output", "Output question added");
    check(oq->checkAnswer(" 1\n 2 ") && !oq->checkAnswer("12"), "Output answers ignore extra spaces but must match");

    MyVector<string> code = Storage::split("Code|Graphs|Dijkstra's Algorithm|Hard|Write it|void f();|||||void f() {}|greedy", '|');
    int codeId = bank.addRecord(code, 100, err);
    check(bank.get(codeId)->needsAiGrading() && !bank.get(outId)->needsAiGrading(), "Only code questions need AI grading");
    check(bank.get(codeId)->toJson().find("void f() {}") == string::npos &&
          oq->toJson().find("1 2\"") == string::npos, "toJson() never contains the answer");

    QuestionBank reloaded;
    reloaded.setFile("test_data/questions.txt");
    check(reloaded.load() == 42, "Questions are saved and loaded again");
    check(reloaded.get(outId)->checkAnswer("1 2") && reloaded.get(outId)->toJson().find("\\t") != string::npos,
          "Multi-line code with tabs survives saving");
    check(reloaded.get(1001)->getCorrectAnswerText() == bank.get(1001)->getCorrectAnswerText(),
          "Reloaded MCQ keeps the same option order");

    // ---------- student statistics
    Student s("ridam@gmail.com", "Ridam", h1, 0);
    check(s.checkPassword("pass1234") && !s.checkPassword("pass123"), "checkPassword");
    s.recordAnswer(bank.get(1001), true, 86400 * 10, "1");
    s.recordAnswer(bank.get(1002), false, 86400 * 10 + 5, "2");
    s.recordAnswer(bank.get(outId), false, 86400 * 11, "12");
    check(s.getTopicAttempts("Arrays") == 2 && s.getTopicAccuracy("Arrays") == 50.0, "Per-topic accuracy");
    check(s.getSubtopicAttempts("Stacks", "Stack Operations") == 1, "Per-subtopic attempts");
    check(s.getWeakestTopic() == "Stacks", "Weakest topic");
    check(s.isMistake(1002) && s.getMistakeCount() == 2 && s.getLastAnswer(outId) == "12", "Mistakes keep your answer");
    s.recordAnswer(bank.get(1002), true, 86400 * 11 + 1, "1");
    check(!s.isMistake(1002) && s.getMistakeCount() == 1, "A later right answer clears the mistake");
    check(s.getRecentAttempts(1)[0].questionId == 1002, "Recent attempts: newest first (stack)");
    MyVector<int> days = s.getActiveDays();
    check(days.size() == 2 && days[0] == 10 && s.getDayAttempts(11) == 2 && s.getDayCorrect(11) == 1,
          "Day-by-day statistics for the progress graph");

    // ---------- revision manager
    RevisionManager rm;
    buildTopicGraph(rm);
    MyVector<string> path = rm.bfs("Arrays");
    check(path.size() == 11 && path[0] == "Arrays" && path[1] == "Stacks", "BFS from Arrays reaches all 11 topics");
    MyVector<MyPair<double, string>> ranked = rm.rankTopics(&s);
    check(ranked.size() == 2 && ranked[0].second == "Stacks", "Heap ranking puts the weakest topic first");
    check(rm.recommendNext(&s) == "Recursion", "Recommendation: first unmastered topic in BFS from Stacks");

    // ---------- quiz session
    QuizSession session;
    MyVector<int> ids = bank.filter("Arrays", "Easy", "MCQ");
    int total = session.start(ids, 3, "practice");
    Question* first = session.next(bank);
    check(total == 3 && first != nullptr && first->getId() == ids[0] && session.getRemaining() == 2,
          "Session queue gives questions in order (FIFO)");

    // ---------- platform: accounts, OTP, import, answers
    clearFolder("test_data");
    {
        Platform p("test_data", "test_data/no_dataset.txt");
        p.init();
        check(p.needsSetup(), "Fresh platform needs an admin");
        check(p.createAdmin("Teacher", "Teacher@College.edu", "teach123", err), "Admin created with email");
        Student* st = p.registerStudent("Ridam Sharma", "ridam@gmail.com", "pass123", err);
        check(st != nullptr && p.registerStudent("Copy", "RIDAM@gmail.com", "pass123", err) == nullptr,
              "One account per email");
        check(p.login("student", "ridam@gmail.com", "pass123", err) == st, "Student login");
        check(p.login("admin", "ridam@gmail.com", "pass123", err) == nullptr, "Student can't log in as admin");
        check(p.login("", "teacher@college.edu", "teach123", err) != nullptr, "Any-role login for the terminal app");

        string code2, name;
        check(!p.startPasswordReset("student", "nobody@gmail.com", "123456", 1000, code2, name) && code2 == "no_account",
              "No OTP for an unknown email");
        check(p.startPasswordReset("student", "ridam@gmail.com", "123456", 1000, code2, name) && name == "Ridam Sharma",
              "OTP started");
        check(!p.startPasswordReset("student", "ridam@gmail.com", "222222", 1030, code2, name) && code2 == "cooldown",
              "Second OTP within a minute is refused");
        check(p.finishPasswordReset("student", "ridam@gmail.com", "123456", "newpass1", 1000 + 601, err) == nullptr,
              "Expired OTP is refused");
        check(p.startPasswordReset("student", "ridam@gmail.com", "654321", 2000, code2, name), "New OTP after the cooldown");
        bool locked = true;
        for (int i = 0; i < 5; i++) {
            locked = p.finishPasswordReset("student", "ridam@gmail.com", "000000", "newpass1", 2001, err) == nullptr && locked;
        }
        check(locked && p.finishPasswordReset("student", "ridam@gmail.com", "654321", "newpass1", 2002, err) == nullptr,
              "Five wrong OTPs lock the code");
        check(p.startPasswordReset("student", "ridam@gmail.com", "777777", 3000, code2, name) &&
              p.finishPasswordReset("student", "ridam@gmail.com", "777777", "123", 3001, err) == nullptr &&
              p.finishPasswordReset("student", "ridam@gmail.com", "777777", "newpass1", 3002, err) == st,
              "Right OTP + valid password changes the password");
        check(p.login("student", "ridam@gmail.com", "newpass1", err) == st &&
              p.login("student", "ridam@gmail.com", "pass123", err) == nullptr, "Only the new password works");

        check(p.importQuestion(mcqFields("Arrays", "Sparse Matrices", "Easy", "Q", 2), err) > 0, "Import a valid question");
        check(p.importQuestion(mcqFields("Arrays", "Dijkstra's Algorithm", "Easy", "Q", 2), err) < 0,
              "Import refuses a subtopic from another topic");
        check(p.importQuestion(mcqFields("Arrays", "Sparse Matrices", "Expert", "Q", 2), err) < 0, "Import refuses a bad level");
        check(p.importQuestion(mcqFields("Arrays", "Sparse Matrices", "Easy", "Q", 5), err) < 0, "Import refuses MCQ answer 5");

        QuizSession qs;
        qs.start(p.getBank().getIds(), 1, "practice");
        qs.next(p.getBank());
        AnswerOutcome o;
        string wrong = p.getBank().get(1001)->checkAnswer("1") ? "2" : "1";
        check(p.submitAnswer(st, qs, wrong, o) && !o.correct && o.myAnswer.substr(0, 1) == wrong, "Wrong answer recorded");
        check(p.getMistakeIds(st).size() == 1 && p.saveNote(st->getEmail(), 1001, "Remember: row-major.\nCheck it."),
              "Mistake listed and its explanation saved");
    }
    {
        Platform p("test_data", "test_data/no_dataset.txt");
        p.init();
        Student* st = p.getStudent("ridam@gmail.com");
        check(st != nullptr && st->getTotalAttempts() == 1 && st->isMistake(1001), "Answers are replayed after a restart");
        check(p.getNote("ridam@gmail.com", 1001) == "Remember: row-major.\nCheck it.", "Saved explanation survives a restart");
        string e2;
        check(p.login("student", "ridam@gmail.com", "newpass1", e2) == st, "Changed password survives a restart");
    }
    // ---------- question set (dataset) for practice
    clearFolder("test_data");
    {
        MyVector<string> lines;
        for (int i = 1; i <= 6; i++) {
            lines.push_back(datasetLine(i, "MCQ", "Stacks", "Stack Operations", "Easy", "1"));
        }
        lines.push_back(datasetLine(7, "MCQ", "Stacks", "Stack Operations", "Hard", "1"));
        lines.push_back(datasetLine(8, "Output", "Stacks", "Stack Operations", "Easy", "5", "cout << 5;"));
        lines.push_back(datasetLine(9, "Code", "Queues", "Circular Queue", "Medium", "int f() { return 0; }"));
        lines.push_back(datasetLine(10, "MCQ", "Stacks", "Not A Subtopic", "Easy", "1"));         // bad syllabus
        lines.push_back(datasetLine(3, "MCQ", "Stacks", "Stack Operations", "Easy", "1"));        // id used twice
        lines.push_back(datasetLine(11, "MCQ", "Stacks", "Stack Operations", "Expert", "1"));     // bad level
        lines.push_back("500050\tMCQ\tonly a few fields");                                       // broken line
        Storage::writeLines("test_data/dataset.txt", lines);
        // An AI file line with an id from the dataset range must be ignored.
        Storage::appendLine("test_data/questions.txt", datasetLine(12, "MCQ", "Arrays", "Sparse Matrices", "Easy", "1"));

        Platform p("test_data", "test_data/dataset.txt");
        p.init();
        QuestionBank& b = p.getBank();
        check(p.getDatasetCount() == 9 && p.getDatasetProblems().size() == 4,
              "Question set: 9 good lines loaded, 4 bad lines skipped and reported");
        check(b.get(DATASET_FIRST_ID + 12) == nullptr, "AI file can't add a question with a dataset id");
        int aiId = p.importQuestion(mcqFields("Stacks", "Stack Operations", "Easy", "AI q", 1), err);
        check(aiId == 1001, "AI ids still start at 1001 when the question set is loaded");

        bool keepsText = true;
        int positions[4] = {0, 0, 0, 0};
        for (int i = 1; i <= 6; i++) {
            string right = b.get(DATASET_FIRST_ID + i)->getCorrectAnswerText();
            keepsText = keepsText && right.substr(3) == "right";
            positions[right[0] - '1']++;
        }
        check(keepsText && (positions[0] < 6), "Question-set MCQ options are shuffled and keep the right text");

        Student* st = p.registerStudent("Asha", "asha@gmail.com", "pass123", err);
        int available = 0, similar = 0;
        MyVector<int> ids = p.pickPractice(st, "Stacks", "Stack Operations", "Easy", "MCQ", 4, available, false, similar);
        check(ids.size() == 4 && available == 6, "Practice picks 4 of the 6 matching questions");
        bool onlyDataset = true;
        for (int i = 0; i < ids.size(); i++) onlyDataset = onlyDataset && QuestionBank::isDatasetId(ids[i]);
        check(onlyDataset, "Practice never uses AI questions");
        ids = p.pickPractice(st, "Stacks", "", "", "", 20, available, false, similar);
        check(ids.size() == 8 && available == 8, "Asking for 20 gives the 8 that exist (a shorter session)");
        ids = p.pickPractice(st, "Graphs", "", "Hard", "Code", 5, available, false, similar);
        check(ids.size() == 0 && available == 0, "No matching questions: empty, so the page can explain");

        // fill: without the AI, a short session is filled from the same topic.
        ids = p.pickPractice(st, "Stacks", "Stack Operations", "Hard", "MCQ", 5, available, true, similar);
        bool allStacksMcq = true;
        for (int i = 0; i < ids.size(); i++) {
            Question* q = b.get(ids[i]);
            allStacksMcq = allStacksMcq && q->getTopic() == "Stacks" && q->getTypeName() == "MCQ";
        }
        check(ids.size() == 5 && available == 1 && similar == 4 && ids[0] == DATASET_FIRST_ID + 7 && allStacksMcq,
              "Fill: 1 Hard MCQ matches, 4 Stacks MCQs of other levels are added after it");
        ids = p.pickPractice(st, "Stacks", "", "Easy", "Code", 20, available, true, similar);
        bool allStacks = true;
        for (int i = 0; i < ids.size(); i++) allStacks = allStacks && b.get(ids[i])->getTopic() == "Stacks";
        check(ids.size() == 8 && available == 0 && similar == 8 && allStacks,
              "Fill never leaves the topic: 8 Stacks MCQ/output questions exist, so 8 (not 20)");
        ids = p.pickPractice(st, "Queues", "", "", "MCQ", 5, available, true, similar);
        check(ids.size() == 0 && similar == 0, "Fill never adds code questions (they need the AI)");

        // Answer 5 of the 6 Easy MCQs: 1-4 right (at times 100..400), 5 wrong; 6 never seen.
        QuizSession qs;
        MyVector<int> order;
        for (int i = 1; i <= 5; i++) order.push_back(DATASET_FIRST_ID + i);
        for (int i = 0; i < 5; i++) {
            Question* q = b.get(order[i]);
            string ans = q->getCorrectAnswerText().substr(0, 1);
            if (i == 4) ans = ans == "1" ? "2" : "1";
            st->recordAnswer(q, i < 4, 100 * (i + 1), ans);
        }
        ids = p.pickPractice(st, "Stacks", "Stack Operations", "Easy", "MCQ", 6, available, false, similar);
        check(ids.size() == 6 && ids[0] == DATASET_FIRST_ID + 6, "Practice: an unseen question comes first");
        check(ids[1] == DATASET_FIRST_ID + 5, "...then the one answered wrong");
        check(ids[2] == DATASET_FIRST_ID + 1 && ids[5] == DATASET_FIRST_ID + 4,
              "...then right answers, the one answered longest ago first (min-heap)");

        // Mixed: the type mix wins over "unseen first", so already-answered
        // output and code questions still appear when unseen MCQs are left.
        st->recordAnswer(b.get(DATASET_FIRST_ID + 8), true, 600, "5");
        st->recordAnswer(b.get(DATASET_FIRST_ID + 9), true, 700, "x");
        ids = p.pickPractice(st, "", "", "", "", 3, available, false, similar);
        bool hasMcq = false, hasOut = false, hasCode = false;
        for (int i = 0; i < ids.size(); i++) {
            string t = b.get(ids[i])->getTypeName();
            hasMcq = hasMcq || t == "MCQ";
            hasOut = hasOut || t == "Output";
            hasCode = hasCode || t == "Code";
        }
        check(ids.size() == 3 && hasMcq && hasOut && hasCode, "Mixed session takes turns between the 3 types");
    }
    {
        Platform p("test_data", "test_data/dataset.txt");
        p.init();
        Platform q("test_data", "test_data/dataset.txt");
        q.init();
        check(p.getBank().get(DATASET_FIRST_ID + 2)->getCorrectAnswerText() ==
              q.getBank().get(DATASET_FIRST_ID + 2)->getCorrectAnswerText(),
              "Question-set option order is the same after every restart");
    }
    clearFolder("test_data");
    remove("test_data");

    cout << "\n";
    if (failures == 0) {
        cout << "All tests passed.\n";
    } else {
        cout << "Some tests FAILED.\n";
    }
    return failures;
}
