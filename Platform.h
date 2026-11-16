#pragma once
#include <string>
#include "MyHashMap.h"
#include "MyHashSet.h"
#include "MyPair.h"
#include "MyStack.h"
#include "QuestionBank.h"
#include "QuizSession.h"
#include "RevisionManager.h"
#include "Syllabus.h"
#include "User.h"
#include "Admin.h"
#include "Student.h"
using namespace std;

// A password-reset code waiting to be used. Only a hash of the code is kept.
struct OtpEntry {
    string codeHash;
    long long sentAt;
    long long expiresAt;
    int triesLeft;
};

class Platform {
private:
    Syllabus syllabus;
    QuestionBank bank;
    RevisionManager revision;
    MyHashMap<string, User*> users;          // email -> account
    MyVector<string> userOrder;              // keeps users.txt in sign-up order
    MyVector<string> studentEmails;
    MyHashMap<int, MyPair<int, int>> questionStats;
    MyHashMap<string, string> notes;         // "email|questionId" -> AI explanation of the mistake
    MyHashMap<string, OtpEntry> otps;        // "role|email" -> pending reset code
    bool adminExists;
    string usersFile;
    string attemptsFile;
    string questionsFile;
    string notesFile;
    string datasetFile;
    MyVector<string> datasetProblems;        // lines of the question set that were skipped, and why
    unsigned int shuffleCount;               // makes every practice session's order different

    void loadDataset();
    bool checkQuestion(MyVector<string>& f, string& error);
    MyVector<int> shuffled(MyVector<int> ids, unsigned int seed);

    void applyAnswer(Student* student, Question* q, bool isCorrect, long long time,
                     string answer, string feedback);
    void loadUsers();
    void loadAttempts();
    void loadNotes();
    string userLine(User* user);
    bool saveUser(User* user);
    bool saveAllUsers();
    bool checkNewAccount(string name, string email, string password, string& error);
    bool recordAnswer(Student* student, QuizSession& session, Question* q, string answer,
                      bool correct, string feedback, AnswerOutcome& out);

public:
    static const int OTP_VALID_SECONDS = 600;
    static const int OTP_COOLDOWN_SECONDS = 60;
    static const int OTP_TRIES = 5;
    static const int MAX_SESSION_QUESTIONS = 20;

    Platform(string dataFolder = "data", string datasetFile = "dataset/questions_dataset.txt");
    ~Platform();

    void init();
    QuestionBank& getBank();
    RevisionManager& getRevision();
    Syllabus& getSyllabus();

    static string normalizeEmail(string email);
    static bool isValidEmail(string email);
    static bool isValidPassword(string password, string& error);

    bool needsSetup();
    bool createAdmin(string name, string email, string password, string& error);
    Student* registerStudent(string name, string email, string password, string& error);
    // role is "student", "admin", or "" (any role, used by the terminal app)
    User* login(string role, string email, string password, string& error);
    User* findUser(string email);

    // Forgot password. The code itself is made by the caller (web_app.py uses
    // Python's secrets module) and sent by email; C++ only keeps its hash.
    bool startPasswordReset(string role, string email, string code, long long now,
                            string& errorCode, string& name);
    User* finishPasswordReset(string role, string email, string code, string newPassword,
                              long long now, string& error);

    Student* getStudent(string email);
    MyVector<Student*> getStudents();

    // fields: type topic subtopic difficulty text code opt1 opt2 opt3 opt4 answer explanation
    int importQuestion(MyVector<string> fields, string& error);

    // Practice questions come from the question set (dataset).
    // Picks up to count ids for the choice ("" = any); available = how many match.
    // fill = true: if too few match, similar questions of the same topic are
    // added (never code questions); similar = how many were added.
    MyVector<int> pickPractice(Student* student, string topic, string subtopic, string level,
                               string type, int count, int& available, bool fill, int& similar);
    int getDatasetCount();
    MyVector<string> getDatasetProblems();

    bool submitAnswer(Student* student, QuizSession& session, string answer, AnswerOutcome& out);
    bool submitGraded(Student* student, QuizSession& session, string answer, bool correct,
                      string feedback, AnswerOutcome& out);

    MyVector<int> getMistakeIds(Student* student);
    MyVector<int> orderForStudent(Student* student, MyVector<int> ids);
    MyVector<int> takeTurns(MyVector<int> ids, string by);   // by: "topic", "level" or "type"
    MyVector<string> getWeakSubtopics(Student* student, string topic, int count);
    MyVector<int> getRecentQuestionIds(string topic, int count);

    string getNote(string email, int questionId);
    bool saveNote(string email, int questionId, string note);

    int getQuestionAttempts(int id);
    double getQuestionAccuracy(int id);
    MyVector<MyPair<double, int>> getMostMissed(int count);
    double getClassTopicAccuracy(string topic, int& attempts);
};
