#include <iostream>
#include <string>
#include "MyStack.h"
#include "MyPair.h"
#include "MyQueue.h"
#include "MyHashMap.h"
#include "MyHashSet.h"
#include "MyPriorityQueue.h"
using namespace std;

int failures = 0;

void check(bool condition, string name) {
    if (condition) {
        cout << "[PASS] " << name << "\n";
    } else {
        cout << "[FAIL] " << name << "\n";
        failures++;
    }
}

int main() {
    MyVector<int> v;
    for (int i = 0; i < 5000; i++) {
        v.push_back(i);
    }
    check(v.size() == 5000 && v[0] == 0 && v[4999] == 4999, "MyVector grows past its first capacity");
    MyVector<int> copyV = v;
    copyV[0] = -5;
    check(v[0] == 0, "MyVector deep copy");
    MyVector<int> s;
    s.push_back(5);
    s.push_back(1);
    s.push_back(4);
    s.push_back(1);
    insertionSort(s);
    check(s[0] == 1 && s[1] == 1 && s[2] == 4 && s[3] == 5, "insertionSort");

    MyStack<int> st;
    for (int i = 1; i <= 2000; i++) {
        st.push(i);
    }
    MyStack<int> stCopy = st;
    check(st.top() == 2000 && st.size() == 2000, "MyStack grows and keeps the last push on top");
    st.pop();
    check(st.top() == 1999 && stCopy.top() == 2000, "MyStack pop + copy independence");
    MyStack<int> empty;
    empty.pop();
    check(empty.empty(), "MyStack pop on empty is safe");

    MyQueue<string> q;
    q.push("a");
    q.push("b");
    q.push("c");
    MyQueue<string> qCopy = q;
    check(q.front() == "a", "MyQueue front is first pushed");
    q.pop();
    q.pop();
    q.pop();
    q.pop();
    check(q.empty() && qCopy.size() == 3, "MyQueue drain + copy independence");

    MyHashMap<int, int> hm;
    int startBuckets = hm.getBucketCount();
    for (int i = 0; i < 1000; i++) {
        hm.insert(i, i * i);
    }
    bool allOk = true;
    for (int i = 0; i < 1000; i++) {
        if (!hm.contains(i) || hm.get(i) != i * i) {
            allOk = false;
        }
    }
    check(allOk && hm.size() == 1000, "MyHashMap 1000 inserts");
    check(hm.getBucketCount() > startBuckets && hm.size() * 4 <= hm.getBucketCount() * 3,
          "MyHashMap rehashes to keep load factor <= 0.75");
    hm.insert(5, -1);
    check(hm.get(5) == -1 && hm.size() == 1000, "MyHashMap overwrite");
    check(!hm.contains(12345), "MyHashMap missing key");
    MyHashMap<string, int> counts;
    counts.get("x") += 2;
    counts.get("x") += 3;
    MyHashMap<string, int> countsCopy = counts;
    counts.get("x") = 0;
    check(countsCopy.get("x") == 5, "MyHashMap deep copy");

    MyHashSet<string> hs;
    hs.insert("Trees");
    hs.insert("Trees");
    check(hs.contains("Trees") && !hs.contains("Graphs") && hs.size() == 1, "MyHashSet");

    check(MyPair<int, string>(1, "b") < MyPair<int, string>(2, "a") &&
          MyPair<int, string>(1, "a") < MyPair<int, string>(1, "b"), "MyPair operator<");

    MyPriorityQueue<MyPair<double, string>> pq;
    pq.push(MyPair<double, string>(50.0, "Stack"));
    pq.push(MyPair<double, string>(0.0, "Trees"));
    pq.push(MyPair<double, string>(100.0, "Arrays"));
    pq.push(MyPair<double, string>(0.0, "Graphs"));
    string order = "";
    while (!pq.empty()) {
        order += pq.top().second + " ";
        pq.pop();
    }
    check(order == "Graphs Trees Stack Arrays ", "MyPriorityQueue min-heap order");

    cout << "\n";
    if (failures == 0) {
        cout << "All tests passed.\n";
    } else {
        cout << "Some tests FAILED.\n";
    }
    return failures;
}
