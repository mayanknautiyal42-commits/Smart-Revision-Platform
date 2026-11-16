#include "Student.h"
using namespace std;

Student::Student(string email, string name, string passwordHash, long long createdAt)
    : User(email, name, passwordHash, createdAt) {
    totalCorrect = 0;
    totalAttempts = 0;
    lastActive = 0;
}

string Student::getRole() {
    return "student";
}

void Student::addTo(MyHashMap<string, MyPair<int, int>>& stats, string key, bool isCorrect) {
    MyPair<int, int>& record = stats.get(key);
    record.second++;
    if (isCorrect) {
        record.first++;
    }
}

int Student::attemptsIn(MyHashMap<string, MyPair<int, int>>& stats, string key) {
    if (!stats.contains(key)) {
        return 0;
    }
    return stats.get(key).second;
}

double Student::accuracyIn(MyHashMap<string, MyPair<int, int>>& stats, string key) {
    if (!stats.contains(key)) {
        return 0.0;
    }
    MyPair<int, int> record = stats.get(key);
    if (record.second == 0) {
        return 0.0;
    }
    return 100.0 * record.first / record.second;
}

void Student::recordAnswer(Question* q, bool isCorrect, long long time, string answer, string feedback) {
    addTo(topicStats, q->getTopic(), isCorrect);
    addTo(subtopicStats, q->getTopic() + "|" + q->getSubtopic(), isCorrect);
    addTo(levelStats, q->getDifficulty(), isCorrect);
    addTo(typeStats, q->getTypeName(), isCorrect);
    addTo(topicLevelStats, q->getTopic() + "|" + q->getDifficulty(), isCorrect);
    MyPair<int, int>& day = dayStats.get((int)(time / 86400));
    day.second++;
    if (isCorrect) {
        day.first++;
    }
    lastResult.insert(q->getId(), isCorrect);
    lastAnswer.insert(q->getId(), answer);
    lastSeen.insert(q->getId(), time);
    lastFeedback.insert(q->getId(), feedback);

    totalAttempts++;
    if (isCorrect) {
        totalCorrect++;
    }
    if (time > lastActive) {
        lastActive = time;
    }

    AttemptRecord record;
    record.questionId = q->getId();
    record.topic = q->getTopic();
    record.subtopic = q->getSubtopic();
    record.difficulty = q->getDifficulty();
    record.type = q->getTypeName();
    record.correct = isCorrect;
    record.time = time;
    recentAttempts.push(record);
}

int Student::getTopicAttempts(string topic) {
    return attemptsIn(topicStats, topic);
}

int Student::getTopicCorrect(string topic) {
    if (!topicStats.contains(topic)) {
        return 0;
    }
    return topicStats.get(topic).first;
}

double Student::getTopicAccuracy(string topic) {
    return accuracyIn(topicStats, topic);
}

int Student::getSubtopicAttempts(string topic, string subtopic) {
    return attemptsIn(subtopicStats, topic + "|" + subtopic);
}

double Student::getSubtopicAccuracy(string topic, string subtopic) {
    return accuracyIn(subtopicStats, topic + "|" + subtopic);
}

MyVector<int> Student::getActiveDays() {
    MyVector<int> days = dayStats.keys();
    insertionSort(days);
    return days;
}

int Student::getDayAttempts(int day) {
    if (!dayStats.contains(day)) {
        return 0;
    }
    return dayStats.get(day).second;
}

int Student::getDayCorrect(int day) {
    if (!dayStats.contains(day)) {
        return 0;
    }
    return dayStats.get(day).first;
}

int Student::getLevelAttempts(string level) {
    return attemptsIn(levelStats, level);
}

double Student::getLevelAccuracy(string level) {
    return accuracyIn(levelStats, level);
}

int Student::getTypeAttempts(string type) {
    return attemptsIn(typeStats, type);
}

double Student::getTypeAccuracy(string type) {
    return accuracyIn(typeStats, type);
}

int Student::getTopicLevelAttempts(string topic, string level) {
    return attemptsIn(topicLevelStats, topic + "|" + level);
}

double Student::getTopicLevelAccuracy(string topic, string level) {
    return accuracyIn(topicLevelStats, topic + "|" + level);
}

MyVector<string> Student::getAttemptedTopics() {
    MyVector<string> topics = topicStats.keys();
    insertionSort(topics);
    return topics;
}

string Student::getWeakestTopic() {
    MyVector<string> topics = getAttemptedTopics();
    string weakest = "";
    double lowest = 0.0;
    for (int i = 0; i < topics.size(); i++) {
        double accuracy = getTopicAccuracy(topics[i]);
        if (weakest == "" || accuracy < lowest) {
            lowest = accuracy;
            weakest = topics[i];
        }
    }
    return weakest;
}

int Student::getTotalAttempts() {
    return totalAttempts;
}

int Student::getTotalCorrect() {
    return totalCorrect;
}

double Student::getOverallAccuracy() {
    if (totalAttempts == 0) {
        return 0.0;
    }
    return 100.0 * totalCorrect / totalAttempts;
}

long long Student::getLastActive() {
    return lastActive;
}

bool Student::hasAttempted(int questionId) {
    return lastResult.contains(questionId);
}

bool Student::isMistake(int questionId) {
    if (!lastResult.contains(questionId)) {
        return false;
    }
    return lastResult.get(questionId) == false;
}

int Student::getMistakeCount() {
    MyVector<int> ids = lastResult.keys();
    int count = 0;
    for (int i = 0; i < ids.size(); i++) {
        if (lastResult.get(ids[i]) == false) {
            count++;
        }
    }
    return count;
}

string Student::getLastAnswer(int questionId) {
    if (!lastAnswer.contains(questionId)) {
        return "";
    }
    return lastAnswer.get(questionId);
}

string Student::getLastFeedback(int questionId) {
    if (!lastFeedback.contains(questionId)) {
        return "";
    }
    return lastFeedback.get(questionId);
}

MyVector<AttemptRecord> Student::getRecentAttempts(int count) {
    MyVector<AttemptRecord> result;
    MyStack<AttemptRecord> copy = recentAttempts;
    while (!copy.empty() && result.size() < count) {
        result.push_back(copy.top());
        copy.pop();
    }
    return result;
}

long long Student::getLastSeen(int questionId) {
    if (!lastSeen.contains(questionId)) {
        return 0;
    }
    return lastSeen.get(questionId);
}
