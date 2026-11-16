#pragma once
#include <string>
#include "User.h"
#include "MyHashMap.h"
#include "MyPair.h"
#include "MyStack.h"
#include "Question.h"
using namespace std;

struct AttemptRecord {
    int questionId;
    string topic;
    string subtopic;
    string difficulty;
    string type;
    bool correct;
    long long time;
};

class Student : public User {
private:
    // MyPair(correct, attempts) for each key
    MyHashMap<string, MyPair<int, int>> topicStats;
    MyHashMap<string, MyPair<int, int>> subtopicStats;   // key "topic|subtopic"
    MyHashMap<string, MyPair<int, int>> levelStats;
    MyHashMap<string, MyPair<int, int>> typeStats;
    MyHashMap<string, MyPair<int, int>> topicLevelStats; // key "topic|level"
    MyHashMap<int, MyPair<int, int>> dayStats;           // key = day number (time / 86400)
    MyHashMap<int, bool> lastResult;
    MyHashMap<int, string> lastAnswer;
    MyHashMap<int, string> lastFeedback;
    MyHashMap<int, long long> lastSeen;                  // question id -> time of the last answer
    MyStack<AttemptRecord> recentAttempts;
    int totalCorrect;
    int totalAttempts;
    long long lastActive;

    void addTo(MyHashMap<string, MyPair<int, int>>& stats, string key, bool isCorrect);
    int attemptsIn(MyHashMap<string, MyPair<int, int>>& stats, string key);
    double accuracyIn(MyHashMap<string, MyPair<int, int>>& stats, string key);

public:
    Student(string email, string name, string passwordHash, long long createdAt);

    string getRole();
    void recordAnswer(Question* q, bool isCorrect, long long time, string answer = "", string feedback = "");

    int getTopicAttempts(string topic);
    int getTopicCorrect(string topic);
    double getTopicAccuracy(string topic);

    int getSubtopicAttempts(string topic, string subtopic);
    double getSubtopicAccuracy(string topic, string subtopic);

    int getLevelAttempts(string level);
    double getLevelAccuracy(string level);

    int getTypeAttempts(string type);
    double getTypeAccuracy(string type);

    int getTopicLevelAttempts(string topic, string level);
    double getTopicLevelAccuracy(string topic, string level);

    MyVector<int> getActiveDays();
    int getDayAttempts(int day);
    int getDayCorrect(int day);

    MyVector<string> getAttemptedTopics();
    string getWeakestTopic();

    int getTotalAttempts();
    int getTotalCorrect();
    double getOverallAccuracy();
    long long getLastActive();

    bool hasAttempted(int questionId);
    bool isMistake(int questionId);
    int getMistakeCount();
    string getLastAnswer(int questionId);
    string getLastFeedback(int questionId);
    long long getLastSeen(int questionId);               // 0 if never answered
    MyVector<AttemptRecord> getRecentAttempts(int count);
};
