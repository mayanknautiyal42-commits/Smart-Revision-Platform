#include "Platform.h"
#include "Storage.h"
#include "InputUtils.h"
#include "MyPriorityQueue.h"
#include <ctime>
#include <cstdlib>
#include <iostream>
using namespace std;

Platform::Platform(string dataFolder, string datasetFile) {
    adminExists = false;
    shuffleCount = 0;
    this->datasetFile = datasetFile;
    Storage::makeFolder(dataFolder);
    usersFile = dataFolder + "/users.txt";
    attemptsFile = dataFolder + "/attempts.txt";
    questionsFile = dataFolder + "/questions.txt";
    notesFile = dataFolder + "/notes.txt";
}

Platform::~Platform() {
    for (int i = 0; i < userOrder.size(); i++) {
        delete users.get(userOrder[i]);
    }
}

void Platform::init() {
    loadSyllabus(syllabus);
    buildTopicGraph(revision);
    loadDataset();                 // the question set first, then the AI questions
    bank.setFile(questionsFile);
    bank.load();
    loadUsers();
    loadAttempts();
    loadNotes();
}

QuestionBank& Platform::getBank() {
    return bank;
}

RevisionManager& Platform::getRevision() {
    return revision;
}

Syllabus& Platform::getSyllabus() {
    return syllabus;
}

/* ---------- Accounts ---------- */

string Platform::normalizeEmail(string email) {
    return toLowerCase(trim(email));
}

// A simple, strict check: name@domain.tld with common characters only.
bool Platform::isValidEmail(string email) {
    if (email.length() < 6 || email.length() > 80) {
        return false;
    }
    int at = -1;
    for (int i = 0; i < (int)email.length(); i++) {
        char c = email[i];
        if (c == '@') {
            if (at != -1) {
                return false;
            }
            at = i;
            continue;
        }
        bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' ||
                  c == '-' || c == '+';
        if (!ok) {
            return false;
        }
    }
    if (at < 1 || at > (int)email.length() - 4) {
        return false;
    }
    string domain = email.substr(at + 1);
    if (domain.find('.') == string::npos || domain.find("..") != string::npos ||
        domain[0] == '.' || domain[0] == '-' || domain[domain.length() - 1] == '.') {
        return false;
    }
    if (email[0] == '.' || email[at - 1] == '.') {
        return false;
    }
    return true;
}

bool Platform::isValidPassword(string password, string& error) {
    if (password.length() < 6 || password.length() > 64 || hasTabOrNewline(password)) {
        error = "Password must be 6 to 64 characters.";
        return false;
    }
    return true;
}

void Platform::loadUsers() {
    MyVector<string> lines = Storage::readLines(usersFile);
    for (int i = 0; i < lines.size(); i++) {
        MyVector<string> f = Storage::split(lines[i], '\t');
        if (f.size() < 5 || users.contains(f[1])) {
            continue;
        }
        long long created = atoll(f[4].c_str());
        if (f[0] == "admin") {
            users.insert(f[1], new Admin(f[1], f[2], f[3], created));
            adminExists = true;
        } else if (f[0] == "student") {
            users.insert(f[1], new Student(f[1], f[2], f[3], created));
            studentEmails.push_back(f[1]);
        } else {
            continue;
        }
        userOrder.push_back(f[1]);
    }
}

void Platform::loadAttempts() {
    MyVector<string> lines = Storage::readLines(attemptsFile);
    for (int i = 0; i < lines.size(); i++) {
        MyVector<string> f = Storage::split(lines[i], '\t');
        if (f.size() < 4) {
            continue;
        }
        Student* student = getStudent(f[0]);
        int id;
        if (student == nullptr || !parseInt(f[1], id)) {
            continue;
        }
        Question* q = bank.get(id);
        if (q == nullptr) {
            continue;
        }
        string answer = f.size() > 4 ? Storage::unescape(f[4]) : "";
        string feedback = f.size() > 5 ? Storage::unescape(f[5]) : "";
        applyAnswer(student, q, f[2] == "1", atoll(f[3].c_str()), answer, feedback);
    }
}

void Platform::loadNotes() {
    MyVector<string> lines = Storage::readLines(notesFile);
    for (int i = 0; i < lines.size(); i++) {
        MyVector<string> f = Storage::split(lines[i], '\t');
        if (f.size() < 4) {
            continue;
        }
        notes.insert(f[0] + "|" + f[1], Storage::unescape(f[3]));
    }
}

string Platform::userLine(User* user) {
    return user->getRole() + "\t" + user->getEmail() + "\t" + user->getName() + "\t" +
           user->getPasswordHash() + "\t" + to_string(user->getCreatedAt());
}

bool Platform::saveUser(User* user) {
    return Storage::appendLine(usersFile, userLine(user));
}

bool Platform::saveAllUsers() {
    MyVector<string> lines;
    for (int i = 0; i < userOrder.size(); i++) {
        lines.push_back(userLine(users.get(userOrder[i])));
    }
    return Storage::writeLines(usersFile, lines);
}

bool Platform::checkNewAccount(string name, string email, string password, string& error) {
    if (name == "" || name.length() > 40 || hasTabOrNewline(name)) {
        error = "Enter your full name (up to 40 characters).";
        return false;
    }
    if (!isValidEmail(email)) {
        error = "Enter a valid email address, like name@example.com.";
        return false;
    }
    if (!isValidPassword(password, error)) {
        return false;
    }
    if (users.contains(email)) {
        error = "An account with this email already exists. Log in, or use Forgot password.";
        return false;
    }
    return true;
}

bool Platform::needsSetup() {
    return !adminExists;
}

bool Platform::createAdmin(string name, string email, string password, string& error) {
    if (adminExists) {
        error = "An admin account already exists.";
        return false;
    }
    email = normalizeEmail(email);
    name = trim(name);
    if (!checkNewAccount(name, email, password, error)) {
        return false;
    }
    Admin* admin = new Admin(email, name, User::hashPassword(email, password), (long long)time(nullptr));
    if (!saveUser(admin)) {
        delete admin;
        error = "Could not save to the data folder.";
        return false;
    }
    users.insert(email, admin);
    userOrder.push_back(email);
    adminExists = true;
    return true;
}

Student* Platform::registerStudent(string name, string email, string password, string& error) {
    email = normalizeEmail(email);
    name = trim(name);
    if (!checkNewAccount(name, email, password, error)) {
        return nullptr;
    }
    Student* student = new Student(email, name, User::hashPassword(email, password), (long long)time(nullptr));
    if (!saveUser(student)) {
        delete student;
        error = "Could not save to the data folder.";
        return nullptr;
    }
    users.insert(email, student);
    userOrder.push_back(email);
    studentEmails.push_back(email);
    return student;
}

User* Platform::findUser(string email) {
    email = normalizeEmail(email);
    if (!users.contains(email)) {
        return nullptr;
    }
    return users.get(email);
}

User* Platform::login(string role, string email, string password, string& error) {
    User* user = findUser(email);
    if (user == nullptr || !user->checkPassword(password)) {
        error = "Wrong email or password.";
        return nullptr;
    }
    if (role != "" && user->getRole() != role) {
        if (user->getRole() == "admin") {
            error = "This is the admin account. Use the Admin login.";
        } else {
            error = "This is a student account. Use the Student login.";
        }
        return nullptr;
    }
    return user;
}

/* ---------- Forgot password (OTP) ---------- */

bool Platform::startPasswordReset(string role, string email, string code, long long now,
                                  string& errorCode, string& name) {
    email = normalizeEmail(email);
    User* user = findUser(email);
    if (user == nullptr || user->getRole() != role) {
        errorCode = "no_account";
        return false;
    }
    string key = role + "|" + email;
    if (otps.contains(key) && now - otps.get(key).sentAt < OTP_COOLDOWN_SECONDS) {
        errorCode = "cooldown";
        return false;
    }
    if (code.length() != 6) {
        errorCode = "bad_code";
        return false;
    }
    OtpEntry entry;
    entry.codeHash = User::hashPassword(key, code);
    entry.sentAt = now;
    entry.expiresAt = now + OTP_VALID_SECONDS;
    entry.triesLeft = OTP_TRIES;
    otps.insert(key, entry);
    name = user->getName();
    return true;
}

User* Platform::finishPasswordReset(string role, string email, string code, string newPassword,
                                    long long now, string& error) {
    email = normalizeEmail(email);
    string key = role + "|" + email;
    if (!otps.contains(key) || otps.get(key).triesLeft <= 0) {
        error = "Request a new OTP first.";
        return nullptr;
    }
    OtpEntry& entry = otps.get(key);
    if (now > entry.expiresAt) {
        entry.triesLeft = 0;
        error = "This OTP has expired. Request a new one.";
        return nullptr;
    }
    if (User::hashPassword(key, trim(code)) != entry.codeHash) {
        entry.triesLeft--;
        if (entry.triesLeft <= 0) {
            error = "Too many wrong attempts. Request a new OTP.";
        } else {
            error = "Wrong OTP. " + to_string(entry.triesLeft) + " tries left.";
        }
        return nullptr;
    }
    if (!isValidPassword(newPassword, error)) {
        return nullptr;   // the OTP stays valid so the student can fix the password
    }
    User* user = findUser(email);
    if (user == nullptr) {
        error = "Account not found.";
        return nullptr;
    }
    string oldHash = user->getPasswordHash();
    user->setPassword(newPassword);
    if (!saveAllUsers()) {
        user->setPasswordHash(oldHash);   // keep memory and file the same
        error = "Could not save the new password.";
        return nullptr;
    }
    entry.triesLeft = 0;
    return user;
}

Student* Platform::getStudent(string email) {
    if (!users.contains(email)) {
        return nullptr;
    }
    User* user = users.get(email);
    if (user->getRole() != "student") {
        return nullptr;
    }
    return (Student*)user;
}

MyVector<Student*> Platform::getStudents() {
    MyVector<Student*> result;
    for (int i = 0; i < studentEmails.size(); i++) {
        result.push_back(getStudent(studentEmails[i]));
    }
    return result;
}

/* ---------- Questions from the AI ---------- */

// The same rules for every question, from the AI or from the question set.
// f: type topic subtopic difficulty text code opt1 opt2 opt3 opt4 answer explanation
bool Platform::checkQuestion(MyVector<string>& f, string& error) {
    if (f.size() != 12) {
        error = "A question needs 12 fields.";
        return false;
    }
    string type = f[0], topic = f[1], subtopic = f[2], level = f[3];
    if (type != "MCQ" && type != "Output" && type != "Code") {
        error = "Unknown question type.";
        return false;
    }
    if (!syllabus.hasSubtopic(topic, subtopic)) {
        error = "Topic or subtopic is not in the syllabus.";
        return false;
    }
    if (level != "Easy" && level != "Medium" && level != "Hard") {
        error = "Level must be Easy, Medium or Hard.";
        return false;
    }
    if (type == "Output" && trim(f[4]) == "") {
        f[4] = "What does this code print?";
    }
    if (trim(f[4]) == "" || trim(f[10]) == "" || trim(f[11]) == "") {
        error = "Question text, answer and explanation are required.";
        return false;
    }
    if (type == "MCQ") {
        for (int k = 6; k <= 9; k++) {
            if (trim(f[k]) == "") {
                error = "An MCQ needs 4 options.";
                return false;
            }
        }
    }
    if (type == "Output" && trim(f[5]) == "") {
        error = "An output question needs code.";
        return false;
    }
    return true;
}

int Platform::importQuestion(MyVector<string> f, string& error) {
    if (!checkQuestion(f, error)) {
        return -1;
    }
    return bank.addRecord(f, (long long)time(nullptr), error);
}

/* ---------- The question set (dataset) ---------- */

// Reads dataset/questions_dataset.txt (made by import_dataset.py). A line
// that breaks a rule is skipped and listed in datasetProblems; one bad line
// never stops the others from loading. Messages go to cerr, because in
// --api mode cout is the channel that web_app.py reads JSON from.
void Platform::loadDataset() {
    MyVector<string> lines = Storage::readLines(datasetFile);
    for (int i = 0; i < lines.size(); i++) {
        MyVector<string> raw = Storage::split(lines[i], '\t');
        MyVector<string> f;
        for (int k = 0; k < raw.size(); k++) {
            f.push_back(Storage::unescape(raw[k]));
        }
        string where = "line " + to_string(i + 1);
        if (f.size() != R_FIELD_COUNT) {
            datasetProblems.push_back(where + ": expected " + to_string(R_FIELD_COUNT) + " fields.");
            continue;
        }
        int id;
        if (parseInt(f[R_ID], id) && QuestionBank::isDatasetId(id)) {
            where = "question id " + to_string(id - DATASET_FIRST_ID);
        }
        MyVector<string> fields;              // the 12 fields checkQuestion() expects
        for (int k = R_TYPE; k < R_FIELD_COUNT; k++) {
            if (k != R_CREATED) {
                fields.push_back(f[k]);
            }
        }
        string error;
        if (!checkQuestion(fields, error)) {
            datasetProblems.push_back(where + ": " + error);
            continue;
        }
        f[R_TEXT] = fields[4];                // checkQuestion() may fill in the Output prompt
        if (!bank.addDatasetRecord(f, error)) {
            datasetProblems.push_back(where + ": " + error);
        }
    }
    if (lines.size() == 0) {
        cerr << "Question set: none loaded (" << datasetFile << " is missing or empty).\n";
    } else {
        cerr << "Question set: " << bank.countDataset() << " questions loaded";
        if (datasetProblems.size() > 0) {
            cerr << ", " << datasetProblems.size() << " skipped:";
        }
        cerr << "\n";
        for (int i = 0; i < datasetProblems.size(); i++) {
            cerr << "  " << datasetProblems[i] << "\n";
        }
    }
}

int Platform::getDatasetCount() {
    return bank.countDataset();
}

MyVector<string> Platform::getDatasetProblems() {
    return datasetProblems;
}

// Fisher-Yates shuffle, using the same mixBits() hash as the MCQ options.
MyVector<int> Platform::shuffled(MyVector<int> ids, unsigned int seed) {
    for (int i = ids.size() - 1; i > 0; i--) {
        int j = (int)(mixBits(seed * 2654435761u + (unsigned int)i) % (unsigned int)(i + 1));
        int temp = ids[i];
        ids[i] = ids[j];
        ids[j] = temp;
    }
    return ids;
}

MyVector<int> Platform::pickPractice(Student* student, string topic, string subtopic, string level,
                                     string type, int count, int& available, bool fill, int& similar) {
    MyVector<int> ids = bank.filterDataset(topic, subtopic, level, type);
    available = ids.size();
    similar = 0;
    MyVector<int> ordered = orderForStudent(student, ids);
    MyVector<int> result;
    MyHashSet<int> taken;
    for (int i = 0; i < ordered.size() && result.size() < count; i++) {
        result.push_back(ordered[i]);
        taken.insert(ordered[i]);
    }
    if (!fill) {
        return result;
    }
    // Too few: widen the choice one step at a time, always inside the same
    // topic: any level, then any subtopic, then any question type. Code
    // questions are never added this way: the student didn't ask for them,
    // and they need the AI to be graded.
    string steps[3][3] = {{subtopic, "", type}, {"", "", type}, {"", "", ""}};
    for (int step = 0; step < 3 && result.size() < count; step++) {
        MyVector<int> wider = bank.filterDataset(topic, steps[step][0], steps[step][1], steps[step][2]);
        MyVector<int> fresh;
        for (int i = 0; i < wider.size(); i++) {
            if (!taken.contains(wider[i]) && !bank.get(wider[i])->needsAiGrading()) {
                fresh.push_back(wider[i]);
            }
        }
        fresh = orderForStudent(student, fresh);
        for (int i = 0; i < fresh.size() && result.size() < count; i++) {
            result.push_back(fresh[i]);
            taken.insert(fresh[i]);
            similar++;
        }
    }
    return result;
}

/* ---------- Answers ---------- */

void Platform::applyAnswer(Student* student, Question* q, bool isCorrect, long long time,
                           string answer, string feedback) {
    student->recordAnswer(q, isCorrect, time, answer, feedback);
    MyPair<int, int>& stats = questionStats.get(q->getId());
    stats.second++;
    if (isCorrect) {
        stats.first++;
    }
}

bool Platform::recordAnswer(Student* student, QuizSession& session, Question* q, string answer,
                            bool correct, string feedback, AnswerOutcome& out) {
    if (answer.length() > 4000) {
        answer = answer.substr(0, 4000);
    }
    out.correct = correct;
    out.correctAnswer = q->getCorrectAnswerText();
    out.explanation = q->getExplanation();
    out.feedback = feedback;
    out.myAnswer = q->describeAnswer(answer);
    out.questionId = q->getId();

    long long now = (long long)time(nullptr);
    applyAnswer(student, q, correct, now, answer, feedback);
    string line = student->getEmail() + "\t" + to_string(q->getId()) + "\t" + (correct ? "1" : "0") +
                  "\t" + to_string(now) + "\t" + Storage::escape(answer) + "\t" + Storage::escape(feedback);
    if (!Storage::appendLine(attemptsFile, line)) {
        cerr << "Warning: could not save the answer to " << attemptsFile << "\n";
    }
    session.recordResult(correct);
    return true;
}

// MCQ and Output questions: C++ checks the answer itself.
bool Platform::submitAnswer(Student* student, QuizSession& session, string answer, AnswerOutcome& out) {
    if (!session.hasCurrent()) {
        return false;
    }
    Question* q = bank.get(session.getCurrentId());
    if (q == nullptr || q->needsAiGrading() || !q->isValidAnswer(answer)) {
        return false;
    }
    return recordAnswer(student, session, q, answer, q->checkAnswer(answer), "", out);
}

// Code questions: the AI has graded the answer (web_app.py), C++ records it.
bool Platform::submitGraded(Student* student, QuizSession& session, string answer, bool correct,
                            string feedback, AnswerOutcome& out) {
    if (!session.hasCurrent()) {
        return false;
    }
    Question* q = bank.get(session.getCurrentId());
    if (q == nullptr || !q->needsAiGrading() || !q->isValidAnswer(answer)) {
        return false;
    }
    return recordAnswer(student, session, q, answer, correct, feedback, out);
}

MyVector<int> Platform::getMistakeIds(Student* student) {
    MyVector<int> all = bank.getIds();
    MyVector<int> result;
    for (int i = all.size() - 1; i >= 0; i--) {   // newest first
        if (student->isMistake(all[i])) {
            result.push_back(all[i]);
        }
    }
    return result;
}

// Round robin: puts the ids in groups (by topic, level or type), then takes
// one from each group in turn. Order inside a group is kept, so the best
// question of each group still comes first.
MyVector<int> Platform::takeTurns(MyVector<int> ids, string by) {
    MyHashMap<string, MyVector<int>> groups;
    MyVector<string> groupNames;
    for (int i = 0; i < ids.size(); i++) {
        Question* q = bank.get(ids[i]);
        string key = by == "type" ? q->getTypeName()
                   : by == "level" ? q->getDifficulty()
                   : q->getTopic() + "|" + q->getSubtopic();
        if (!groups.contains(key)) {
            groupNames.push_back(key);
        }
        groups.get(key).push_back(ids[i]);
    }

    MyVector<int> result;
    int round = 0;
    while (result.size() < ids.size()) {
        for (int g = 0; g < groupNames.size(); g++) {
            MyVector<int>& group = groups.get(groupNames[g]);
            if (round < group.size()) {
                result.push_back(group[round]);
            }
        }
        round++;
    }
    return result;
}

// Order for a practice session. Best first:
//   1. questions the student has never answered (random order),
//   2. questions they got wrong last time (random order),
//   3. questions they got right, the one answered longest ago first (min-heap).
// Each group is spread over topics/subtopics. Then the list takes turns by
// level and finally by type, so "All levels" and "Mixed" really are mixed,
// and inside each type the best question still comes first.
MyVector<int> Platform::orderForStudent(Student* student, MyVector<int> ids) {
    shuffleCount++;
    unsigned int seed = mixBits((unsigned int)time(nullptr) ^ (shuffleCount * 40503u) ^
                                (unsigned int)hashKey(student->getEmail(), 1 << 30));
    ids = shuffled(ids, seed);

    MyVector<int> fresh, wrong;
    MyPriorityQueue<MyPair<long long, int>> oldestFirst;   // (last seen, position after shuffling)
    for (int i = 0; i < ids.size(); i++) {
        if (!student->hasAttempted(ids[i])) {
            fresh.push_back(ids[i]);
        } else if (student->isMistake(ids[i])) {
            wrong.push_back(ids[i]);
        } else {
            oldestFirst.push(MyPair<long long, int>(student->getLastSeen(ids[i]), i));
        }
    }
    MyVector<int> right;
    while (!oldestFirst.empty()) {
        right.push_back(ids[oldestFirst.top().second]);
        oldestFirst.pop();
    }
    fresh = takeTurns(fresh, "topic");
    wrong = takeTurns(wrong, "topic");
    right = takeTurns(right, "topic");

    MyVector<int> result;
    for (int i = 0; i < fresh.size(); i++) result.push_back(fresh[i]);
    for (int i = 0; i < wrong.size(); i++) result.push_back(wrong[i]);
    for (int i = 0; i < right.size(); i++) result.push_back(right[i]);
    return takeTurns(takeTurns(result, "level"), "type");
}

// Subtopics of one topic below the mastery level, weakest first (min-heap).
MyVector<string> Platform::getWeakSubtopics(Student* student, string topic, int count) {
    MyPriorityQueue<MyPair<double, string>> heap;
    MyVector<string> subs = syllabus.getSubtopics(topic);
    for (int i = 0; i < subs.size(); i++) {
        if (student->getSubtopicAttempts(topic, subs[i]) == 0) {
            continue;
        }
        double accuracy = student->getSubtopicAccuracy(topic, subs[i]);
        if (accuracy < RevisionManager::MASTERY) {
            heap.push(MyPair<double, string>(accuracy, subs[i]));
        }
    }
    MyVector<string> result;
    while (!heap.empty() && result.size() < count) {
        result.push_back(heap.top().second);
        heap.pop();
    }
    return result;
}

// Newest questions of a topic, so the AI can be told not to repeat them.
MyVector<int> Platform::getRecentQuestionIds(string topic, int count) {
    MyVector<int> all = bank.getIds();
    MyVector<int> result;
    for (int i = all.size() - 1; i >= 0 && result.size() < count; i--) {
        if (topic == "" || bank.get(all[i])->getTopic() == topic) {
            result.push_back(all[i]);
        }
    }
    return result;
}

string Platform::getNote(string email, int questionId) {
    string key = email + "|" + to_string(questionId);
    if (!notes.contains(key)) {
        return "";
    }
    return notes.get(key);
}

bool Platform::saveNote(string email, int questionId, string note) {
    string line = email + "\t" + to_string(questionId) + "\t" + to_string((long long)time(nullptr)) +
                  "\t" + Storage::escape(note);
    if (!Storage::appendLine(notesFile, line)) {
        return false;
    }
    notes.insert(email + "|" + to_string(questionId), note);
    return true;
}

/* ---------- Class statistics ---------- */

int Platform::getQuestionAttempts(int id) {
    if (!questionStats.contains(id)) {
        return 0;
    }
    return questionStats.get(id).second;
}

double Platform::getQuestionAccuracy(int id) {
    if (!questionStats.contains(id)) {
        return 0.0;
    }
    MyPair<int, int> stats = questionStats.get(id);
    if (stats.second == 0) {
        return 0.0;
    }
    return 100.0 * stats.first / stats.second;
}

MyVector<MyPair<double, int>> Platform::getMostMissed(int count) {
    MyPriorityQueue<MyPair<double, int>> heap;
    MyVector<int> ids = questionStats.keys();
    for (int i = 0; i < ids.size(); i++) {
        double accuracy = getQuestionAccuracy(ids[i]);
        if (accuracy < 100.0) {
            heap.push(MyPair<double, int>(accuracy, ids[i]));
        }
    }
    MyVector<MyPair<double, int>> result;
    while (!heap.empty() && result.size() < count) {
        result.push_back(heap.top());
        heap.pop();
    }
    return result;
}

double Platform::getClassTopicAccuracy(string topic, int& attempts) {
    int correct = 0;
    attempts = 0;
    MyVector<Student*> students = getStudents();
    for (int i = 0; i < students.size(); i++) {
        attempts += students[i]->getTopicAttempts(topic);
        correct += students[i]->getTopicCorrect(topic);
    }
    if (attempts == 0) {
        return 0.0;
    }
    return 100.0 * correct / attempts;
}
