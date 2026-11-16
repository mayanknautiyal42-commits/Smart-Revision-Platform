#include "Syllabus.h"
#include "Storage.h"
using namespace std;

// subtopicList is separated by '|'
void Syllabus::addTopic(string topic, string subtopicList) {
    if (!subtopics.contains(topic)) {
        topics.push_back(topic);
    }
    subtopics.insert(topic, Storage::split(subtopicList, '|'));
}

MyVector<string> Syllabus::getTopics() {
    return topics;
}

MyVector<string> Syllabus::getSubtopics(string topic) {
    if (!subtopics.contains(topic)) {
        return MyVector<string>();
    }
    return subtopics.get(topic);
}

bool Syllabus::hasTopic(string topic) {
    return subtopics.contains(topic);
}

bool Syllabus::hasSubtopic(string topic, string subtopic) {
    MyVector<string> list = getSubtopics(topic);
    for (int i = 0; i < list.size(); i++) {
        if (list[i] == subtopic) {
            return true;
        }
    }
    return false;
}

void loadSyllabus(Syllabus& s) {
    s.addTopic("Arrays",
        "Single and Multidimensional Arrays|Address Calculation|Applications of Arrays|Sparse Matrices");
    s.addTopic("Stacks",
        "Array Implementation of Stack|Stack Operations|Infix to Prefix and Postfix Conversion|"
        "Evaluation of Postfix Expression");
    s.addTopic("Recursion",
        "Recursion and its Types");
    s.addTopic("Queues",
        "Array Implementation of Queue|Queue Operations|Circular Queue|Deque|Priority Queue");
    s.addTopic("Linked List",
        "Singly Linked List|Two-way Header List|Linked Stack and Queue|Doubly Linked List|"
        "Circular Linked List|Linked List in Array|Polynomial Representation and Addition|"
        "Generalized Linked List");
    s.addTopic("Trees",
        "Basic Terminology|Binary Trees|Binary Tree Representation|Algebraic Expressions|"
        "Complete Binary Tree|Binary Heaps|Extended Binary Trees|Array and Linked Representation|"
        "Threaded Binary Trees|Traversal of Binary and Threaded Trees|Huffman Algorithm and Huffman Tree");
    s.addTopic("Binary Search Trees",
        "Binary Search Tree Basics|Insertion and Deletion in BST|Complexity of Search Algorithm|"
        "Path Length|AVL Trees");
    s.addTopic("Sorting",
        "Types and Properties of Sorting|Insertion Sort|Bubble Sort|Quick Sort|Two-Way Merge Sort|Heap Sort");
    s.addTopic("Searching and Hashing",
        "Sequential Search|Binary Search|Comparison and Analysis|Hash Table|Hash Functions|"
        "Collision Resolution Strategies|Hash Table Implementation");
    s.addTopic("File Structures",
        "File Organization|Organization of Records into Blocks|Indexing|B Tree Index Files|B+ Tree Index Files");
    s.addTopic("Graphs",
        "DFS and BFS Traversal|Disjoint Sets: Union-Find|Path Compression|Union by Rank/Size|"
        "Kruskal's Algorithm|Prim's Algorithm|Dijkstra's Algorithm|Bellman-Ford Algorithm|"
        "Floyd-Warshall Algorithm");
}

// "A -> B" means: once A is understood, B builds on it.
void buildTopicGraph(RevisionManager& revision) {
    revision.addRelation("Arrays", "Stacks");
    revision.addRelation("Arrays", "Queues");
    revision.addRelation("Arrays", "Linked List");
    revision.addRelation("Arrays", "Sorting");
    revision.addRelation("Stacks", "Recursion");
    revision.addRelation("Linked List", "Trees");
    revision.addRelation("Queues", "Graphs");
    revision.addRelation("Recursion", "Trees");
    revision.addRelation("Recursion", "Sorting");
    revision.addRelation("Sorting", "Searching and Hashing");
    revision.addRelation("Trees", "Binary Search Trees");
    revision.addRelation("Trees", "Graphs");
    revision.addRelation("Searching and Hashing", "Binary Search Trees");
    revision.addRelation("Searching and Hashing", "File Structures");
    revision.addRelation("Binary Search Trees", "File Structures");
}
