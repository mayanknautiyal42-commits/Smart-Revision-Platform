#include "RevisionManager.h"
#include "MyPriorityQueue.h"
#include "MyQueue.h"
#include "MyHashSet.h"
using namespace std;

void RevisionManager::addNode(string topic) {
    if (!graph.contains(topic)) {
        graph.insert(topic, MyVector<string>());
        nodes.push_back(topic);
    }
}

void RevisionManager::addRelation(string from, string to) {
    addNode(from);
    addNode(to);
    graph.get(from).push_back(to);
}

MyVector<string> RevisionManager::getNodes() {
    return nodes;
}

MyVector<MyPair<string, string>> RevisionManager::getEdges() {
    MyVector<MyPair<string, string>> edges;
    for (int i = 0; i < nodes.size(); i++) {
        MyVector<string> neighbours = graph.get(nodes[i]);
        for (int j = 0; j < neighbours.size(); j++) {
            edges.push_back(MyPair<string, string>(nodes[i], neighbours[j]));
        }
    }
    return edges;
}

MyVector<string> RevisionManager::bfs(string start) {
    MyVector<string> visitOrder;
    if (!graph.contains(start)) {
        visitOrder.push_back(start);
        return visitOrder;
    }

    MyHashSet<string> visited;
    MyQueue<string> queue;
    queue.push(start);
    visited.insert(start);

    while (!queue.empty()) {
        string current = queue.front();
        queue.pop();
        visitOrder.push_back(current);

        MyVector<string> neighbours = graph.get(current);
        for (int i = 0; i < neighbours.size(); i++) {
            if (!visited.contains(neighbours[i])) {
                visited.insert(neighbours[i]);
                queue.push(neighbours[i]);
            }
        }
    }
    return visitOrder;
}

MyVector<MyPair<double, string>> RevisionManager::rankTopics(Student* student) {
    MyPriorityQueue<MyPair<double, string>> heap;
    MyVector<string> topics = student->getAttemptedTopics();
    for (int i = 0; i < topics.size(); i++) {
        heap.push(MyPair<double, string>(student->getTopicAccuracy(topics[i]), topics[i]));
    }

    MyVector<MyPair<double, string>> ranked;
    while (!heap.empty()) {
        ranked.push_back(heap.top());
        heap.pop();
    }
    return ranked;
}

string RevisionManager::recommendNext(Student* student) {
    MyVector<MyPair<double, string>> ranked = rankTopics(student);
    if (ranked.empty()) {
        return "";
    }
    string weakest = ranked[0].second;
    MyVector<string> path = bfs(weakest);
    for (int i = 1; i < path.size(); i++) {
        string topic = path[i];
        if (student->getTopicAttempts(topic) == 0 || student->getTopicAccuracy(topic) < MASTERY) {
            return topic;
        }
    }
    return "";
}

string RevisionManager::levelSuggestion(Student* student, string topic) {
    int easyTries = student->getTopicLevelAttempts(topic, "Easy");
    int mediumTries = student->getTopicLevelAttempts(topic, "Medium");
    int hardTries = student->getTopicLevelAttempts(topic, "Hard");
    double easy = student->getTopicLevelAccuracy(topic, "Easy");
    double medium = student->getTopicLevelAccuracy(topic, "Medium");

    if (easyTries >= 3 && easy < 50) {
        return "Keep practising Easy";
    }
    if (mediumTries >= 3 && medium >= 80 && hardTries < 3) {
        return "Ready for Hard";
    }
    if (easyTries >= 3 && easy >= 80 && mediumTries < 3) {
        return "Ready for Medium";
    }
    return "";
}
