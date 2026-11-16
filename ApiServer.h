#pragma once
#include <string>
#include "MyHashMap.h"
#include "MyStack.h"
#include "Platform.h"
#include "QuizSession.h"
using namespace std;

struct ApiSession {
    User* user;
    QuizSession quiz;
};

// Reads one command per line (fields separated by TAB, each field escaped
// with Storage::escape) and writes one line of JSON per command.
//
// Commands marked "internal" are only sent by web_app.py itself; the Python
// bridge never forwards them from the browser (see ALLOWED_COMMANDS there).
class ApiServer {
private:
    Platform& platform;
    MyHashMap<string, ApiSession*> sessions;

    string makeToken();
    ApiSession* findSession(string token);
    void endSessionsOf(User* user);
    string error(string code, string message);
    int percent(double value);
    string pick(string value);
    string studentSummaryJson(Student* s);
    string studentReportJson(Student* s);
    string newSessionJson(User* user);
    string outcomeJson(AnswerOutcome& out, QuizSession& quiz);

    string handle(MyVector<string> f);
    string cmdHello();
    string cmdSetup(MyVector<string> f);
    string cmdRegister(MyVector<string> f);
    string cmdLogin(MyVector<string> f);
    string cmdLogout(MyVector<string> f);
    string cmdOtpStart(MyVector<string> f);            // internal
    string cmdResetPassword(MyVector<string> f);
    string cmdImportQuestion(MyVector<string> f);      // internal

    string cmdSyllabus(Student* s);
    string checkChoice(Student* s, MyVector<string> f, string& mode, string& topic, string& subtopic,
                       string& level, string& type, int& count);
    string cmdPrepare(Student* s, MyVector<string> f); // internal
    string cmdPracticeStart(ApiSession* session, Student* s, MyVector<string> f); // internal
    string cmdStartIds(ApiSession* session, MyVector<string> f); // internal
    string cmdAppendIds(ApiSession* session, MyVector<string> f); // internal
    MyVector<int> parseIds(string csv);
    string cmdReview(ApiSession* session, Student* s, MyVector<string> f);
    string cmdNext(ApiSession* session);
    string cmdCurrentInfo(ApiSession* session);        // internal
    string cmdAnswer(ApiSession* session, Student* s, MyVector<string> f);
    string cmdAnswerGraded(ApiSession* session, Student* s, MyVector<string> f); // internal
    string cmdMistakes(Student* s);
    string cmdMistakeInfo(Student* s, MyVector<string> f); // internal
    string cmdSaveNote(Student* s, MyVector<string> f);    // internal
    string cmdPlan(Student* s);
    string cmdAdminOverview();
    string cmdAdminStudent(MyVector<string> f);

public:
    ApiServer(Platform& platform);
    ~ApiServer();
    void run();
};
