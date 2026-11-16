#pragma once
#include <string>
#include "MyHashMap.h"
#include "MyStack.h"
#include "RevisionManager.h"
using namespace std;

// The DSA syllabus: 11 topics, each with its subtopics. The AI writes
// questions for these; C++ refuses a question for anything not listed here.
class Syllabus {
private:
    MyVector<string> topics;
    MyHashMap<string, MyVector<string>> subtopics;

public:
    void addTopic(string topic, string subtopicList);
    MyVector<string> getTopics();
    MyVector<string> getSubtopics(string topic);
    bool hasTopic(string topic);
    bool hasSubtopic(string topic, string subtopic);
};

void loadSyllabus(Syllabus& syllabus);
void buildTopicGraph(RevisionManager& revision);
