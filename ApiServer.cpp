#include "ApiServer.h"
#include "Json.h"
#include "InputUtils.h"
#include "Storage.h"
#include <ctime>
#include <iostream>
#include <random>
using namespace std;

ApiServer::ApiServer(Platform& platform) : platform(platform) {
}

ApiServer::~ApiServer() {
    MyVector<string> tokens = sessions.keys();
    for (int i = 0; i < tokens.size(); i++) {
        delete sessions.get(tokens[i]);
    }
}

void ApiServer::run() {
    string line;
    while (readLine(line)) {
        MyVector<string> raw = Storage::split(line, '\t');
        if (raw[0] == "") {
            continue;
        }
        if (raw[0] == "quit") {
            break;
        }
        MyVector<string> fields;
        for (int i = 0; i < raw.size(); i++) {
            fields.push_back(Storage::unescape(raw[i]));
        }
        cout << handle(fields) << "\n";
        cout.flush();
    }
}

// Several entropy sources: on some older MinGW compilers random_device
// returns the same numbers every run, so time and clock are mixed in.
string ApiServer::makeToken() {
    static random_device device;
    static seed_seq seeds{(unsigned)device(), (unsigned)device(), (unsigned)time(nullptr),
                          (unsigned)clock(), (unsigned)(size_t)&device};
    static mt19937 generator(seeds);
    string hex = "0123456789abcdef";
    string token = "";
    for (int i = 0; i < 32; i++) {
        token += hex[generator() % 16];
    }
    return token;
}

ApiSession* ApiServer::findSession(string token) {
    if (token == "" || !sessions.contains(token)) {
        return nullptr;
    }
    return sessions.get(token);
}

// After a password reset, every open login of that account is closed.
void ApiServer::endSessionsOf(User* user) {
    MyVector<string> tokens = sessions.keys();
    for (int i = 0; i < tokens.size(); i++) {
        ApiSession* session = sessions.get(tokens[i]);
        if (session != nullptr && session->user == user) {
            delete session;
            sessions.insert(tokens[i], nullptr);
        }
    }
}

string ApiServer::error(string code, string message) {
    return JsonObject().addBool("ok", false).addStr("code", code).addStr("error", message).str();
}

int ApiServer::percent(double value) {
    return (int)(value + 0.5);
}

string ApiServer::pick(string value) {
    if (value == "*" || value == "All" || value == "Mixed") {
        return "";
    }
    return value;
}

string ApiServer::handle(MyVector<string> f) {
    string cmd = f[0];
    if (cmd == "hello") return cmdHello();
    if (cmd == "setup") return cmdSetup(f);
    if (cmd == "register") return cmdRegister(f);
    if (cmd == "login") return cmdLogin(f);
    if (cmd == "logout") return cmdLogout(f);
    if (cmd == "otp_start") return cmdOtpStart(f);
    if (cmd == "reset_password") return cmdResetPassword(f);
    if (cmd == "import_question") return cmdImportQuestion(f);

    if (f.size() < 2) {
        return error("login_required", "Log in first.");
    }
    ApiSession* session = findSession(f[1]);
    if (session == nullptr) {
        return error("login_required", "Your session has ended. Log in again.");
    }

    if (session->user->getRole() == "admin") {
        if (cmd == "admin_overview") return cmdAdminOverview();
        if (cmd == "admin_student") return cmdAdminStudent(f);
        return error("forbidden", "This page is for students.");
    }

    Student* s = (Student*)session->user;
    if (cmd == "syllabus") return cmdSyllabus(s);
    if (cmd == "prepare") return cmdPrepare(s, f);
    if (cmd == "practice_start") return cmdPracticeStart(session, s, f);
    if (cmd == "start_ids") return cmdStartIds(session, f);
    if (cmd == "append_ids") return cmdAppendIds(session, f);
    if (cmd == "review") return cmdReview(session, s, f);
    if (cmd == "next") return cmdNext(session);
    if (cmd == "current_info") return cmdCurrentInfo(session);
    if (cmd == "answer") return cmdAnswer(session, s, f);
    if (cmd == "answer_graded") return cmdAnswerGraded(session, s, f);
    if (cmd == "mistakes") return cmdMistakes(s);
    if (cmd == "mistake_info") return cmdMistakeInfo(s, f);
    if (cmd == "save_note") return cmdSaveNote(s, f);
    if (cmd == "report") return JsonObject().addBool("ok", true).addJson("report", studentReportJson(s)).str();
    if (cmd == "plan") return cmdPlan(s);
    if (cmd == "admin_overview" || cmd == "admin_student") {
        return error("forbidden", "Only the admin can open this page.");
    }
    return error("unknown_command", "Unknown command: " + cmd);
}

/* ---------- Accounts ---------- */

string ApiServer::cmdHello() {
    return JsonObject().addBool("ok", true)
        .addStr("app", "Smart Revision Platform")
        .addBool("needsSetup", platform.needsSetup())
        .addInt("datasetQuestions", platform.getDatasetCount())
        .addJson("topics", JsonArray::ofStrings(platform.getSyllabus().getTopics())).str();
}

string ApiServer::newSessionJson(User* user) {
    string token = makeToken();
    ApiSession* session = new ApiSession();
    session->user = user;
    sessions.insert(token, session);
    return JsonObject().addBool("ok", true)
        .addStr("token", token)
        .addStr("role", user->getRole())
        .addStr("name", user->getName())
        .addStr("email", user->getEmail()).str();
}

string ApiServer::cmdSetup(MyVector<string> f) {
    if (f.size() < 4) {
        return error("bad_request", "Enter your full name, email and password.");
    }
    string err;
    if (!platform.createAdmin(f[1], f[2], f[3], err)) {
        return error("bad_request", err);
    }
    return newSessionJson(platform.findUser(f[2]));
}

string ApiServer::cmdRegister(MyVector<string> f) {
    if (f.size() < 4) {
        return error("bad_request", "Enter your full name, email and password.");
    }
    string err;
    Student* s = platform.registerStudent(f[1], f[2], f[3], err);
    if (s == nullptr) {
        return error("bad_request", err);
    }
    return newSessionJson(s);
}

string ApiServer::cmdLogin(MyVector<string> f) {
    if (f.size() < 4 || (f[1] != "student" && f[1] != "admin")) {
        return error("bad_request", "Enter your email and password.");
    }
    string err;
    User* user = platform.login(f[1], f[2], f[3], err);
    if (user == nullptr) {
        return error("bad_login", err);
    }
    return newSessionJson(user);
}

string ApiServer::cmdLogout(MyVector<string> f) {
    ApiSession* session = nullptr;
    if (f.size() > 1) {
        session = findSession(f[1]);
    }
    if (session != nullptr) {
        delete session;
        sessions.insert(f[1], nullptr);
    }
    return JsonObject().addBool("ok", true).str();
}

// internal: otp_start role email code
string ApiServer::cmdOtpStart(MyVector<string> f) {
    if (f.size() < 4) {
        return error("bad_request", "Missing fields.");
    }
    string errorCode, name;
    if (!platform.startPasswordReset(f[1], f[2], f[3], (long long)time(nullptr), errorCode, name)) {
        return error(errorCode, errorCode == "cooldown"
            ? "An OTP was sent less than a minute ago. Wait a moment, then try again."
            : "No account for that email.");
    }
    return JsonObject().addBool("ok", true).addStr("name", name)
        .addStr("email", Platform::normalizeEmail(f[2]))
        .addInt("validMinutes", Platform::OTP_VALID_SECONDS / 60).str();
}

// reset_password role email otp newPassword
string ApiServer::cmdResetPassword(MyVector<string> f) {
    if (f.size() < 5 || (f[1] != "student" && f[1] != "admin")) {
        return error("bad_request", "Enter the OTP and a new password.");
    }
    string err;
    User* user = platform.finishPasswordReset(f[1], f[2], f[3], f[4], (long long)time(nullptr), err);
    if (user == nullptr) {
        return error("bad_otp", err);
    }
    endSessionsOf(user);
    return JsonObject().addBool("ok", true).str();
}

// internal: import_question + the 12 record fields
string ApiServer::cmdImportQuestion(MyVector<string> f) {
    MyVector<string> fields;
    for (int i = 1; i < f.size(); i++) {
        fields.push_back(f[i]);
    }
    string err;
    int id = platform.importQuestion(fields, err);
    if (id < 0) {
        return error("bad_question", err);
    }
    return JsonObject().addBool("ok", true).addInt("id", id).str();
}

/* ---------- Student: practice ---------- */

string ApiServer::cmdSyllabus(Student* s) {
    Syllabus& syllabus = platform.getSyllabus();
    MyVector<string> topics = syllabus.getTopics();
    JsonArray list;
    for (int i = 0; i < topics.size(); i++) {
        string t = topics[i];
        list.addJson(JsonObject()
            .addStr("name", t)
            .addJson("subtopics", JsonArray::ofStrings(syllabus.getSubtopics(t)))
            .addInt("attempts", s->getTopicAttempts(t))
            .addInt("accuracy", percent(s->getTopicAccuracy(t)))
            .addStr("suggestion", platform.getRevision().levelSuggestion(s, t)).str());
    }
    return JsonObject().addBool("ok", true)
        .addJson("topics", list.str())
        .addInt("mistakes", s->getMistakeCount()).str();
}

// Checks a choice sent as: cmd token mode topic subtopic level type count.
// Returns "" if it is fine (and fills in the values), or an error reply.
string ApiServer::checkChoice(Student* s, MyVector<string> f, string& mode, string& topic, string& subtopic,
                              string& level, string& type, int& count) {
    if (f.size() < 8) {
        return error("bad_request", "Choose a topic, level, type and number of questions.");
    }
    Syllabus& syllabus = platform.getSyllabus();
    mode = f[2];
    topic = pick(f[3]);
    subtopic = pick(f[4]);
    level = pick(f[5]);
    type = pick(f[6]);
    if (mode != "practice" && mode != "revision") {
        return error("bad_request", "Unknown mode.");
    }
    if (!parseInt(f[7], count) || count < 1 || count > Platform::MAX_SESSION_QUESTIONS) {
        return error("bad_request", "Choose 1 to 20 questions.");
    }
    if (level != "" && level != "Easy" && level != "Medium" && level != "Hard") {
        return error("bad_request", "Level must be Easy, Medium, Hard or All.");
    }
    if (type != "" && type != "MCQ" && type != "Output" && type != "Code") {
        return error("bad_request", "Type must be MCQ, Code question, Predict output or Mixed.");
    }
    if (mode == "revision" && topic == "") {
        MyVector<MyPair<double, string>> ranked = platform.getRevision().rankTopics(s);
        if (ranked.empty()) {
            return error("no_data", "Practise a few questions first, then revision can target your weak spots.");
        }
        topic = ranked[0].second;
    }
    if (topic != "" && !syllabus.hasTopic(topic)) {
        return error("bad_request", "Unknown topic.");
    }
    if (subtopic != "" && (topic == "" || !syllabus.hasSubtopic(topic, subtopic))) {
        return error("bad_request", "Unknown subtopic for this topic.");
    }
    return "";
}

// internal: practice_start token practice topic subtopic level type count [fill]
// Practice uses the question set only (never the AI). If fewer questions
// match than were asked for, fill adds similar questions of the same topic;
// if there are still too few, the session is shorter ("available" says why).
string ApiServer::cmdPracticeStart(ApiSession* session, Student* s, MyVector<string> f) {
    string mode, topic, subtopic, level, type;
    int count;
    string problem = checkChoice(s, f, mode, topic, subtopic, level, type, count);
    if (problem != "") {
        return problem;
    }
    if (mode != "practice") {
        return error("bad_request", "Only practice sessions come from the question set.");
    }
    bool fill = f.size() > 8 && f[8] == "fill";      // web_app.py sends "fill" when the AI can't help
    int available = 0, similar = 0;
    MyVector<int> ids = platform.pickPractice(s, topic, subtopic, level, type, count, available, fill, similar);
    if (ids.empty()) {
        string typeName = type == "Output" ? "predict-output " : type == "Code" ? "code " : type == "MCQ" ? "MCQ " : "";
        string what = (level == "" ? "" : level + " ") + typeName;
        string where = topic == "" ? "any topic" : topic + (subtopic == "" ? "" : " > " + subtopic);
        string msg = platform.getDatasetCount() == 0
            ? "No practice questions have been added yet. (Admin: put the question sheet in the "
              "dataset folder and start the website again.)"
            : "The question set has no " + what + "questions for " + where + " yet. "
              "Try another level, type or subtopic.";
        return error("no_questions", msg);
    }
    JsonArray idList;
    for (int i = 0; i < ids.size(); i++) {
        idList.addJson(to_string(ids[i]));
    }
    int total = session->quiz.start(ids, count, "practice");
    return JsonObject().addBool("ok", true)
        .addJson("ids", idList.str())
        .addInt("total", total)
        .addInt("serial", session->quiz.getSerial())
        .addInt("requested", count)
        .addInt("available", available)
        .addInt("similar", similar)
        .addStr("source", "dataset")
        .addStr("topic", topic).str();
}

// internal: prepare token mode topic subtopic level type count
// Checks the student's choice and gives Python what it needs to write the AI prompt.
string ApiServer::cmdPrepare(Student* s, MyVector<string> f) {
    string mode, topic, subtopic, level, type;
    int count;
    string problem = checkChoice(s, f, mode, topic, subtopic, level, type, count);
    if (problem != "") {
        return problem;
    }
    Syllabus& syllabus = platform.getSyllabus();

    MyVector<string> chosen;
    if (topic == "") {
        chosen = syllabus.getTopics();
    } else {
        chosen.push_back(topic);
    }

    JsonArray topicList;
    JsonArray avoid;
    JsonArray mistakes;
    for (int i = 0; i < chosen.size(); i++) {
        MyVector<string> subs;
        if (subtopic != "") {
            subs.push_back(subtopic);
        } else {
            subs = syllabus.getSubtopics(chosen[i]);
        }
        topicList.addJson(JsonObject()
            .addStr("name", chosen[i])
            .addJson("subtopics", JsonArray::ofStrings(subs))
            .addJson("focus", JsonArray::ofStrings(platform.getWeakSubtopics(s, chosen[i], 3))).str());

        int perTopic = topic == "" ? 3 : 25;
        MyVector<int> recent = platform.getRecentQuestionIds(chosen[i], perTopic);
        for (int k = 0; k < recent.size(); k++) {
            avoid.addStr(platform.getBank().get(recent[k])->getTitle());
        }
    }
    MyVector<int> wrong = platform.getMistakeIds(s);
    int added = 0;
    for (int i = 0; i < wrong.size() && added < 8; i++) {
        Question* q = platform.getBank().get(wrong[i]);
        if (topic == "" || q->getTopic() == topic) {
            mistakes.addJson(JsonObject().addStr("topic", q->getTopic()).addStr("subtopic", q->getSubtopic())
                .addStr("title", q->getTitle()).str());
            added++;
        }
    }
    return JsonObject().addBool("ok", true)
        .addStr("mode", mode)
        .addStr("level", level)
        .addStr("type", type)
        .addInt("count", count)
        .addJson("topics", topicList.str())
        .addJson("avoid", avoid.str())
        .addJson("mistakes", mistakes.str()).str();
}

// internal: start_ids token mode id,id,id
string ApiServer::cmdStartIds(ApiSession* session, MyVector<string> f) {
    if (f.size() < 4) {
        return error("bad_request", "Missing question ids.");
    }
    MyVector<int> ids = parseIds(f[3]);
    int total = session->quiz.start(ids, Platform::MAX_SESSION_QUESTIONS, f[2] == "revision" ? "revision" : "practice");
    if (total == 0) {
        return error("no_questions", "No questions could be prepared. Try again.");
    }
    return JsonObject().addBool("ok", true).addInt("total", total).addInt("serial", session->quiz.getSerial()).str();
}

// internal: append_ids token serial id,id,id
string ApiServer::cmdAppendIds(ApiSession* session, MyVector<string> f) {
    int serial;
    if (f.size() < 4 || !parseInt(f[2], serial)) {
        return error("bad_request", "Missing session or ids.");
    }
    if (serial != session->quiz.getSerial()) {
        return error("stale", "The student has started another session.");
    }
    MyVector<int> ids = parseIds(f[3]);
    int room = Platform::MAX_SESSION_QUESTIONS - session->quiz.getTotal();
    MyVector<int> allowed;
    for (int i = 0; i < ids.size() && i < room; i++) {
        allowed.push_back(ids[i]);
    }
    session->quiz.append(allowed);
    return JsonObject().addBool("ok", true).addInt("added", allowed.size())
        .addInt("total", session->quiz.getTotal()).str();
}

MyVector<int> ApiServer::parseIds(string csv) {
    MyVector<string> parts = Storage::split(csv, ',');
    MyVector<int> ids;
    for (int i = 0; i < parts.size(); i++) {
        int id;
        if (parseInt(parts[i], id) && platform.getBank().get(id) != nullptr) {
            ids.push_back(id);
        }
    }
    return ids;
}

string ApiServer::cmdReview(ApiSession* session, Student* s, MyVector<string> f) {
    int count = 10;
    if (f.size() > 2) {
        parseInt(f[2], count);
    }
    if (count < 1 || count > Platform::MAX_SESSION_QUESTIONS) {
        count = 10;
    }
    MyVector<int> ids = platform.getMistakeIds(s);
    int total = session->quiz.start(ids, count, "review");
    if (total == 0) {
        return error("no_mistakes", "No mistakes to review. Nice work.");
    }
    return JsonObject().addBool("ok", true).addInt("total", total).str();
}

string ApiServer::cmdNext(ApiSession* session) {
    Question* q = session->quiz.next(platform.getBank());
    JsonObject o;
    o.addBool("ok", true)
     .addBool("done", q == nullptr)
     .addInt("number", session->quiz.getNumber())
     .addInt("total", session->quiz.getTotal())
     .addInt("correct", session->quiz.getCorrect())
     .addInt("attempted", session->quiz.getAttempted())
     .addStr("mode", session->quiz.getMode());
    if (q != nullptr) {
        o.addJson("question", q->toJson());
    }
    return o.str();
}

// internal: what Python needs to grade a code answer with the AI
string ApiServer::cmdCurrentInfo(ApiSession* session) {
    if (!session->quiz.hasCurrent()) {
        return error("no_question", "There is no question waiting for an answer.");
    }
    Question* q = platform.getBank().get(session->quiz.getCurrentId());
    return JsonObject().addBool("ok", true)
        .addBool("needsAi", q->needsAiGrading())
        .addJson("question", q->toJson())
        .addStr("reference", q->getCorrectAnswerText())
        .addStr("explanation", q->getExplanation()).str();
}

string ApiServer::outcomeJson(AnswerOutcome& out, QuizSession& quiz) {
    return JsonObject().addBool("ok", true)
        .addInt("id", out.questionId)
        .addBool("correct", out.correct)
        .addStr("correctAnswer", out.correctAnswer)
        .addStr("explanation", out.explanation)
        .addStr("feedback", out.feedback)
        .addStr("myAnswer", out.myAnswer)
        .addInt("score", quiz.getCorrect())
        .addInt("attempted", quiz.getAttempted())
        .addInt("total", quiz.getTotal())
        .addInt("remaining", quiz.getRemaining()).str();
}

string ApiServer::cmdAnswer(ApiSession* session, Student* s, MyVector<string> f) {
    if (!session->quiz.hasCurrent()) {
        return error("no_question", "There is no question waiting for an answer.");
    }
    string answer = f.size() > 2 ? f[2] : "";
    AnswerOutcome out;
    if (!platform.submitAnswer(s, session->quiz, answer, out)) {
        return error("invalid_answer", "That answer isn't valid for this question.");
    }
    return outcomeJson(out, session->quiz);
}

// internal: answer_graded token 1|0 answer feedback
string ApiServer::cmdAnswerGraded(ApiSession* session, Student* s, MyVector<string> f) {
    if (f.size() < 5) {
        return error("bad_request", "Missing grading result.");
    }
    AnswerOutcome out;
    if (!platform.submitGraded(s, session->quiz, f[3], f[2] == "1", f[4], out)) {
        return error("invalid_answer", "Write some code first.");
    }
    return outcomeJson(out, session->quiz);
}

/* ---------- Student: mistakes ---------- */

string ApiServer::cmdMistakes(Student* s) {
    MyVector<int> ids = platform.getMistakeIds(s);
    JsonArray list;
    for (int i = 0; i < ids.size() && i < 60; i++) {
        Question* q = platform.getBank().get(ids[i]);
        list.addJson(JsonObject()
            .addJson("question", q->toJson())
            .addStr("myAnswer", q->describeAnswer(s->getLastAnswer(q->getId())))
            .addStr("correctAnswer", q->getCorrectAnswerText())
            .addStr("explanation", q->getExplanation())
            .addStr("feedback", s->getLastFeedback(q->getId()))
            .addStr("note", platform.getNote(s->getEmail(), q->getId())).str());
    }
    return JsonObject().addBool("ok", true).addInt("count", ids.size()).addJson("mistakes", list.str()).str();
}

// internal: mistake_info token questionId
string ApiServer::cmdMistakeInfo(Student* s, MyVector<string> f) {
    int id;
    if (f.size() < 3 || !parseInt(f[2], id) || platform.getBank().get(id) == nullptr) {
        return error("not_found", "Question not found.");
    }
    if (!s->isMistake(id)) {
        return error("not_mistake", "Your last answer to this question was right, so there is nothing to explain.");
    }
    Question* q = platform.getBank().get(id);
    return JsonObject().addBool("ok", true)
        .addJson("question", q->toJson())
        .addStr("myAnswer", q->describeAnswer(s->getLastAnswer(id)))
        .addStr("correctAnswer", q->getCorrectAnswerText())
        .addStr("explanation", q->getExplanation())
        .addStr("feedback", s->getLastFeedback(id))
        .addStr("note", platform.getNote(s->getEmail(), id)).str();
}

// internal: save_note token questionId note
string ApiServer::cmdSaveNote(Student* s, MyVector<string> f) {
    int id;
    if (f.size() < 4 || !parseInt(f[2], id) || !s->hasAttempted(id)) {
        return error("not_found", "Question not found.");
    }
    if (!platform.saveNote(s->getEmail(), id, f[3])) {
        return error("save_failed", "Could not save the explanation.");
    }
    return JsonObject().addBool("ok", true).str();
}

/* ---------- Reports ---------- */

string ApiServer::studentSummaryJson(Student* s) {
    return JsonObject()
        .addStr("email", s->getEmail())
        .addStr("name", s->getName())
        .addInt("attempts", s->getTotalAttempts())
        .addInt("correct", s->getTotalCorrect())
        .addInt("accuracy", percent(s->getOverallAccuracy()))
        .addStr("weakest", s->getWeakestTopic())
        .addInt("mistakes", s->getMistakeCount())
        .addInt("lastActive", s->getLastActive())
        .addInt("joined", s->getCreatedAt()).str();
}

string ApiServer::studentReportJson(Student* s) {
    QuestionBank& bank = platform.getBank();
    Syllabus& syllabus = platform.getSyllabus();
    MyVector<string> topics = syllabus.getTopics();
    string levelNames[3] = {"Easy", "Medium", "Hard"};

    JsonArray topicList;
    for (int i = 0; i < topics.size(); i++) {
        string t = topics[i];
        JsonArray levels;
        for (int k = 0; k < 3; k++) {
            levels.addJson(JsonObject()
                .addStr("name", levelNames[k])
                .addInt("attempts", s->getTopicLevelAttempts(t, levelNames[k]))
                .addInt("accuracy", percent(s->getTopicLevelAccuracy(t, levelNames[k]))).str());
        }
        JsonArray subs;
        MyVector<string> subNames = syllabus.getSubtopics(t);
        for (int k = 0; k < subNames.size(); k++) {
            int attempts = s->getSubtopicAttempts(t, subNames[k]);
            if (attempts > 0) {
                subs.addJson(JsonObject()
                    .addStr("name", subNames[k])
                    .addInt("attempts", attempts)
                    .addInt("accuracy", percent(s->getSubtopicAccuracy(t, subNames[k]))).str());
            }
        }
        topicList.addJson(JsonObject()
            .addStr("name", t)
            .addInt("attempts", s->getTopicAttempts(t))
            .addInt("correct", s->getTopicCorrect(t))
            .addInt("accuracy", percent(s->getTopicAccuracy(t)))
            .addStr("suggestion", platform.getRevision().levelSuggestion(s, t))
            .addJson("levels", levels.str())
            .addJson("subtopics", subs.str()).str());
    }

    JsonArray levelList;
    for (int k = 0; k < 3; k++) {
        levelList.addJson(JsonObject()
            .addStr("name", levelNames[k])
            .addInt("attempts", s->getLevelAttempts(levelNames[k]))
            .addInt("accuracy", percent(s->getLevelAccuracy(levelNames[k]))).str());
    }

    string typeNames[3] = {"MCQ", "Output", "Code"};
    JsonArray typeList;
    for (int k = 0; k < 3; k++) {
        typeList.addJson(JsonObject()
            .addStr("name", typeNames[k])
            .addInt("attempts", s->getTypeAttempts(typeNames[k]))
            .addInt("accuracy", percent(s->getTypeAccuracy(typeNames[k]))).str());
    }

    // the last 14 days the student practised, oldest first
    JsonArray timeline;
    MyVector<int> days = s->getActiveDays();
    int first = days.size() > 14 ? days.size() - 14 : 0;
    for (int i = first; i < days.size(); i++) {
        timeline.addJson(JsonObject()
            .addInt("day", days[i])
            .addInt("attempts", s->getDayAttempts(days[i]))
            .addInt("correct", s->getDayCorrect(days[i])).str());
    }

    JsonArray recent;
    MyVector<AttemptRecord> attempts = s->getRecentAttempts(8);
    for (int i = 0; i < attempts.size(); i++) {
        AttemptRecord a = attempts[i];
        Question* q = bank.get(a.questionId);
        recent.addJson(JsonObject()
            .addInt("id", a.questionId)
            .addStr("topic", a.topic)
            .addStr("subtopic", a.subtopic)
            .addStr("difficulty", a.difficulty)
            .addStr("type", a.type)
            .addStr("title", q != nullptr ? q->getTitle() : "")
            .addBool("correct", a.correct)
            .addInt("time", a.time).str());
    }

    return JsonObject()
        .addJson("summary", studentSummaryJson(s))
        .addJson("topics", topicList.str())
        .addJson("levels", levelList.str())
        .addJson("types", typeList.str())
        .addJson("timeline", timeline.str())
        .addJson("recent", recent.str()).str();
}

string ApiServer::cmdPlan(Student* s) {
    RevisionManager& revision = platform.getRevision();
    MyVector<MyPair<double, string>> ranked = revision.rankTopics(s);

    JsonArray edges;
    MyVector<MyPair<string, string>> edgeList = revision.getEdges();
    for (int i = 0; i < edgeList.size(); i++) {
        edges.addJson(JsonObject().addStr("from", edgeList[i].first).addStr("to", edgeList[i].second).str());
    }

    JsonObject o;
    o.addBool("ok", true)
     .addBool("hasData", !ranked.empty())
     .addJson("nodes", JsonArray::ofStrings(revision.getNodes()))
     .addJson("edges", edges.str())
     .addInt("mastery", RevisionManager::MASTERY);
    if (ranked.empty()) {
        return o.str();
    }

    JsonArray ranking;
    for (int i = 0; i < ranked.size(); i++) {
        ranking.addJson(JsonObject()
            .addStr("topic", ranked[i].second)
            .addInt("accuracy", percent(ranked[i].first))
            .addInt("attempts", s->getTopicAttempts(ranked[i].second))
            .addJson("weakSubtopics", JsonArray::ofStrings(platform.getWeakSubtopics(s, ranked[i].second, 2))).str());
    }
    string weakest = ranked[0].second;
    string next = revision.recommendNext(s);
    o.addJson("ranking", ranking.str())
     .addStr("weakest", weakest)
     .addJson("weakSubtopics", JsonArray::ofStrings(platform.getWeakSubtopics(s, weakest, 3)))
     .addJson("path", JsonArray::ofStrings(revision.bfs(weakest)));
    if (next == "") {
        o.addNull("next");
    } else {
        o.addStr("next", next);
    }
    return o.str();
}

string ApiServer::cmdAdminOverview() {
    MyVector<Student*> students = platform.getStudents();
    JsonArray list;
    int totalAttempts = 0;
    int totalCorrect = 0;
    int active = 0;
    for (int i = 0; i < students.size(); i++) {
        list.addJson(studentSummaryJson(students[i]));
        totalAttempts += students[i]->getTotalAttempts();
        totalCorrect += students[i]->getTotalCorrect();
        if (students[i]->getTotalAttempts() > 0) {
            active++;
        }
    }

    JsonArray topicList;
    MyVector<string> topics = platform.getSyllabus().getTopics();
    for (int i = 0; i < topics.size(); i++) {
        int attempts;
        double accuracy = platform.getClassTopicAccuracy(topics[i], attempts);
        topicList.addJson(JsonObject()
            .addStr("name", topics[i])
            .addInt("attempts", attempts)
            .addInt("accuracy", percent(accuracy)).str());
    }

    JsonArray missed;
    MyVector<MyPair<double, int>> worst = platform.getMostMissed(8);
    for (int i = 0; i < worst.size(); i++) {
        Question* q = platform.getBank().get(worst[i].second);
        missed.addJson(JsonObject()
            .addInt("id", q->getId())
            .addStr("topic", q->getTopic())
            .addStr("subtopic", q->getSubtopic())
            .addStr("difficulty", q->getDifficulty())
            .addStr("type", q->getTypeName())
            .addStr("title", q->getTitle())
            .addInt("attempts", platform.getQuestionAttempts(q->getId()))
            .addInt("accuracy", percent(worst[i].first)).str());
    }

    int classAccuracy = 0;
    if (totalAttempts > 0) {
        classAccuracy = percent(100.0 * totalCorrect / totalAttempts);
    }
    return JsonObject().addBool("ok", true)
        .addInt("studentCount", students.size())
        .addInt("activeCount", active)
        .addInt("totalAttempts", totalAttempts)
        .addInt("accuracy", classAccuracy)
        .addInt("questionsGenerated", platform.getBank().size() - platform.getDatasetCount())
        .addInt("datasetQuestions", platform.getDatasetCount())
        .addJson("students", list.str())
        .addJson("topics", topicList.str())
        .addJson("mostMissed", missed.str()).str();
}

string ApiServer::cmdAdminStudent(MyVector<string> f) {
    if (f.size() < 3) {
        return error("bad_request", "Choose a student.");
    }
    Student* s = platform.getStudent(Platform::normalizeEmail(f[2]));
    if (s == nullptr) {
        return error("not_found", "No student with that email.");
    }
    return JsonObject().addBool("ok", true).addJson("report", studentReportJson(s)).str();
}
