#pragma once
#include <string>
#include "MyHashMap.h"
#include "MyPair.h"
#include "MyStack.h"
#include "Student.h"
using namespace std;

class RevisionManager {
private:
    MyHashMap<string, MyVector<string>> graph;
    MyVector<string> nodes;

    void addNode(string topic);

public:
    static const int MASTERY = 75;

    void addRelation(string from, string to);
    MyVector<string> getNodes();
    MyVector<MyPair<string, string>> getEdges();

    MyVector<string> bfs(string start);
    MyVector<MyPair<double, string>> rankTopics(Student* student);
    string recommendNext(Student* student);
    string levelSuggestion(Student* student, string topic);
};
