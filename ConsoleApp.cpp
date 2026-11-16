#include "ConsoleApp.h"
#include "InputUtils.h"
#include <iostream>
using namespace std;

ConsoleApp::ConsoleApp(Platform& platform) : platform(platform) {
}

bool ConsoleApp::askChoice(string label, int minVal, int maxVal, int& out) {
    string line;
    while (true) {
        cout << label << " (" << minVal << "-" << maxVal << "): ";
        if (!readLine(line)) {
            return false;
        }
        if (parseInt(line, out) && out >= minVal && out <= maxVal) {
            return true;
        }
        cout << "Please enter a number from " << minVal << " to " << maxVal << ".\n";
    }
}

bool ConsoleApp::askText(string label, string& out) {
    cout << label << " ";
    if (!readLine(out)) {
        return false;
    }
    out = trim(out);
    return true;
}

string ConsoleApp::percentText(double p) {
    return to_string((int)(p + 0.5)) + "%";
}

void ConsoleApp::run() {
    cout << "==============================================\n";
    cout << "        SMART REVISION PLATFORM\n";
    cout << "==============================================\n";
    cout << platform.getSyllabus().getTopics().size() << " DSA topics.\n";
    cout << "New questions are written by the AI on the website (python web_app.py).\n";
    cout << "Here you can practise the questions that were already generated.\n";

    if (platform.needsSetup()) {
        cout << "\nFirst run: create the admin account.\n";
        if (!setupAdmin()) {
            return;
        }
    }

    while (true) {
        cout << "\n=== Welcome ===\n";
        cout << "  [1] Log in\n";
        cout << "  [2] Create a student account\n";
        cout << "  [0] Exit\n";
        int choice;
        if (!askChoice("Choose", 0, 2, choice) || choice == 0) {
            break;
        }
        bool keepGoing = true;
        if (choice == 1) {
            keepGoing = loginUser();
        }
        if (choice == 2) {
            keepGoing = registerStudent();
        }
        if (!keepGoing) {
            break;
        }
    }
    cout << "\nGoodbye!\n";
}

bool ConsoleApp::setupAdmin() {
    while (true) {
        string name, email, password, err;
        if (!askText("Admin full name:", name)) return false;
        if (!askText("Admin email:", email)) return false;
        if (!askText("Admin password (6+ characters):", password)) return false;
        if (platform.createAdmin(name, email, password, err)) {
            cout << "Admin account created.\n";
            return true;
        }
        cout << err << "\n";
    }
}

bool ConsoleApp::registerStudent() {
    string name, email, password, err;
    if (!askText("Your full name:", name)) return false;
    if (!askText("Your email:", email)) return false;
    if (!askText("Choose a password (6+ characters):", password)) return false;
    Student* s = platform.registerStudent(name, email, password, err);
    if (s == nullptr) {
        cout << err << "\n";
        return true;
    }
    cout << "Account created. Welcome, " << s->getName() << "!\n";
    return studentMenu(s);
}

bool ConsoleApp::loginUser() {
    string email, password, err;
    if (!askText("Email:", email)) return false;
    if (!askText("Password:", password)) return false;
    User* user = platform.login("", email, password, err);
    if (user == nullptr) {
        cout << err << "\n";
        return true;
    }
    cout << "Welcome, " << user->getName() << "!\n";
    if (user->getRole() == "admin") {
        return adminMenu();
    }
    return studentMenu((Student*)user);
}

bool ConsoleApp::studentMenu(Student* s) {
    while (true) {
        cout << "\n=== Student menu (" << s->getName() << ") ===\n";
        cout << "  [1] Practise\n";
        cout << "  [2] Review my mistakes (" << s->getMistakeCount() << ")\n";
        cout << "  [3] My report\n";
        cout << "  [4] Revision plan\n";
        cout << "  [5] Recent attempts\n";
        cout << "  [0] Log out\n";
        int choice;
        if (!askChoice("Choose", 0, 5, choice)) return false;
        if (choice == 0) return true;
        if (choice == 1 && !practise(s)) return false;
        if (choice == 2 && !reviewMistakes(s)) return false;
        if (choice == 3) showReport(s);
        if (choice == 4) showPlan(s);
        if (choice == 5) showRecent(s);
    }
}

bool ConsoleApp::practise(Student* s) {
    QuestionBank& bank = platform.getBank();
    MyVector<string> topics = platform.getSyllabus().getTopics();

    cout << "\n=== Choose a topic ===\n";
    for (int i = 0; i < topics.size(); i++) {
        cout << "  [" << (i + 1) << "] " << topics[i] << "\n";
    }
    int allOption = topics.size() + 1;
    cout << "  [" << allOption << "] All topics\n  [0] Back\n";
    int choice;
    if (!askChoice("Topic", 0, allOption, choice)) return false;
    if (choice == 0) return true;
    string topic = "";
    if (choice != allOption) {
        topic = topics[choice - 1];
    }

    cout << "\n=== Choose a level ===\n";
    cout << "  [1] Easy\n  [2] Medium\n  [3] Hard\n  [4] All levels\n";
    int level;
    if (!askChoice("Level", 1, 4, level)) return false;
    string levelNames[5] = {"", "Easy", "Medium", "Hard", ""};
    string difficulty = levelNames[level];

    cout << "\n=== Choose a question type ===\n";
    cout << "  [1] MCQ\n  [2] Predict output\n  [3] Mixed\n";
    cout << "  (Code questions need AI grading, so they are only on the website.)\n";
    int type;
    if (!askChoice("Type", 1, 3, type)) return false;
    string typeNames[4] = {"", "MCQ", "Output", ""};

    MyVector<int> ids = withoutCodeQuestions(bank.filter(topic, difficulty, typeNames[type]));
    if (ids.empty()) {
        cout << "No saved questions for this choice yet. Generate some on the website first.\n";
        return true;
    }
    int count;
    int most = ids.size() < 20 ? ids.size() : 20;
    if (!askChoice("How many questions?", 1, most, count)) return false;
    session.start(platform.orderForStudent(s, ids), count, "practice");
    return runSession(s);
}

MyVector<int> ConsoleApp::withoutCodeQuestions(MyVector<int> ids) {
    MyVector<int> result;
    for (int i = 0; i < ids.size(); i++) {
        if (!platform.getBank().get(ids[i])->needsAiGrading()) {
            result.push_back(ids[i]);
        }
    }
    return result;
}

bool ConsoleApp::reviewMistakes(Student* s) {
    MyVector<int> ids = withoutCodeQuestions(platform.getMistakeIds(s));
    if (ids.empty()) {
        cout << "\nNo MCQ or output mistakes to review here. (Code mistakes: use the website.)\n";
        return true;
    }
    session.start(ids, ids.size(), "review");
    cout << "\nReviewing " << ids.size() << " question(s) you got wrong last time.\n";
    return runSession(s);
}

bool ConsoleApp::runSession(Student* s) {
    Question* q = session.next(platform.getBank());
    while (q != nullptr) {
        q->display(session.getNumber(), session.getTotal());
        AnswerOutcome out;
        string answer;
        while (true) {
            cout << "\n" << q->getAnswerPrompt() << " ";
            if (!readLine(answer)) return false;
            answer = trim(answer);
            if (answer == "q" || answer == "Q") break;
            if (platform.submitAnswer(s, session, answer, out)) break;
            cout << "That's not a valid answer, please try again.\n";
        }
        if (answer == "q" || answer == "Q") {
            cout << "\nYou ended the session early.\n";
            break;
        }
        if (out.correct) {
            cout << "\nCorrect!\n";
        } else {
            cout << "\nNot quite. Correct answer: " << out.correctAnswer << "\n";
        }
        cout << "Why: " << out.explanation << "\n";
        q = session.next(platform.getBank());
    }
    cout << "\n=== Session complete ===\n";
    cout << "Score: " << session.getCorrect() << " / " << session.getAttempted() << "\n";
    return true;
}

void ConsoleApp::showReport(Student* s) {
    cout << "\n=== Report for " << s->getName() << " ===\n";
    if (s->getTotalAttempts() == 0) {
        cout << "No answers yet. Practise a topic first.\n";
        return;
    }
    MyVector<string> topics = platform.getSyllabus().getTopics();
    for (int i = 0; i < topics.size(); i++) {
        string t = topics[i];
        cout << "  " << t << ": ";
        if (s->getTopicAttempts(t) == 0) {
            cout << "not attempted\n";
            continue;
        }
        cout << s->getTopicCorrect(t) << "/" << s->getTopicAttempts(t)
             << " (" << percentText(s->getTopicAccuracy(t)) << ")";
        string tip = platform.getRevision().levelSuggestion(s, t);
        if (tip != "") {
            cout << "  -> " << tip;
        }
        cout << "\n";
    }
    string levels[3] = {"Easy", "Medium", "Hard"};
    cout << "\n  By level:";
    for (int k = 0; k < 3; k++) {
        cout << "  " << levels[k] << " " << percentText(s->getLevelAccuracy(levels[k]))
             << " (" << s->getLevelAttempts(levels[k]) << ")";
    }
    cout << "\n  By type:   MCQ " << percentText(s->getTypeAccuracy("MCQ"))
         << " (" << s->getTypeAttempts("MCQ") << ")  Predict output " << percentText(s->getTypeAccuracy("Output"))
         << " (" << s->getTypeAttempts("Output") << ")  Code " << percentText(s->getTypeAccuracy("Code"))
         << " (" << s->getTypeAttempts("Code") << ")\n";
    cout << "  Overall: " << s->getTotalCorrect() << "/" << s->getTotalAttempts()
         << " (" << percentText(s->getOverallAccuracy()) << ")\n";
    cout << "  Weakest topic: " << s->getWeakestTopic() << "\n";
}

void ConsoleApp::showPlan(Student* s) {
    RevisionManager& revision = platform.getRevision();
    MyVector<MyPair<double, string>> ranked = revision.rankTopics(s);
    cout << "\n=== Revision plan ===\n";
    if (ranked.empty()) {
        cout << "Practise a few questions first.\n";
        return;
    }
    cout << "Weakest first (min-heap):\n";
    for (int i = 0; i < ranked.size(); i++) {
        cout << "  " << (i + 1) << ". " << ranked[i].second << "  " << percentText(ranked[i].first) << "\n";
    }
    MyVector<string> path = revision.bfs(ranked[0].second);
    cout << "Related topics (BFS from " << ranked[0].second << "): ";
    for (int i = 0; i < path.size(); i++) {
        if (i > 0) cout << " -> ";
        cout << path[i];
    }
    string next = revision.recommendNext(s);
    cout << "\nRevise " << ranked[0].second << " first";
    if (next != "") {
        cout << ", then " << next;
    }
    cout << ".\n";
}

void ConsoleApp::showRecent(Student* s) {
    MyVector<AttemptRecord> recent = s->getRecentAttempts(5);
    cout << "\n=== Last " << recent.size() << " attempts (latest first) ===\n";
    if (recent.empty()) {
        cout << "None yet.\n";
    }
    for (int i = 0; i < recent.size(); i++) {
        if (recent[i].correct) cout << "  [right]";
        else cout << "  [wrong]";
        cout << "  Q" << recent[i].questionId << "  " << recent[i].topic << " | "
             << recent[i].difficulty << " | " << recent[i].type << "\n";
    }
}

bool ConsoleApp::adminMenu() {
    while (true) {
        cout << "\n=== Admin menu ===\n";
        cout << "  [1] All students\n";
        cout << "  [2] One student's report\n";
        cout << "  [3] Most-missed questions\n";
        cout << "  [0] Log out\n";
        int choice;
        if (!askChoice("Choose", 0, 3, choice)) return false;
        if (choice == 0) return true;
        if (choice == 1) showStudents();
        if (choice == 2 && !showStudentDetails()) return false;
        if (choice == 3) showMostMissed();
    }
}

void ConsoleApp::showStudents() {
    MyVector<Student*> students = platform.getStudents();
    cout << "\n=== Students (" << students.size() << ") ===\n";
    if (students.empty()) {
        cout << "No students have signed up yet.\n";
    }
    for (int i = 0; i < students.size(); i++) {
        Student* s = students[i];
        cout << "  " << s->getEmail() << " (" << s->getName() << "): "
             << s->getTotalAttempts() << " answers, " << percentText(s->getOverallAccuracy());
        if (s->getWeakestTopic() != "") {
            cout << ", weakest " << s->getWeakestTopic();
        }
        cout << "\n";
    }
}

bool ConsoleApp::showStudentDetails() {
    string email;
    if (!askText("Student email:", email)) return false;
    Student* s = platform.getStudent(Platform::normalizeEmail(email));
    if (s == nullptr) {
        cout << "No student with that email.\n";
        return true;
    }
    showReport(s);
    return true;
}

void ConsoleApp::showMostMissed() {
    MyVector<MyPair<double, int>> worst = platform.getMostMissed(5);
    cout << "\n=== Most-missed questions (min-heap) ===\n";
    if (worst.empty()) {
        cout << "No answers yet.\n";
    }
    for (int i = 0; i < worst.size(); i++) {
        Question* q = platform.getBank().get(worst[i].second);
        cout << "  Q" << q->getId() << " [" << q->getTopic() << " | " << q->getDifficulty() << "] "
             << percentText(worst[i].first) << " correct of " << platform.getQuestionAttempts(q->getId())
             << ": " << q->getTitle() << "\n";
    }
}
