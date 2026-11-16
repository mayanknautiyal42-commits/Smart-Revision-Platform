#pragma once

template <typename T>
class MyQueue {
private:
    struct Node {
        T value;
        Node* next;
    };

    Node* head;
    Node* tail;
    int count;

    void copyFrom(const MyQueue& other) {
        Node* current = other.head;
        while (current != nullptr) {
            push(current->value);
            current = current->next;
        }
    }

public:
    MyQueue() {
        head = nullptr;
        tail = nullptr;
        count = 0;
    }

    MyQueue(const MyQueue& other) {
        head = nullptr;
        tail = nullptr;
        count = 0;
        copyFrom(other);
    }

    MyQueue& operator=(const MyQueue& other) {
        if (this != &other) {
            clear();
            copyFrom(other);
        }
        return *this;
    }

    ~MyQueue() {
        clear();
    }

    void push(T value) {
        Node* node = new Node;
        node->value = value;
        node->next = nullptr;
        if (tail == nullptr) {
            head = node;
        } else {
            tail->next = node;
        }
        tail = node;
        count++;
    }

    void pop() {
        if (head == nullptr) {
            return;
        }
        Node* oldHead = head;
        head = head->next;
        if (head == nullptr) {
            tail = nullptr;
        }
        delete oldHead;
        count--;
    }

    T& front() {
        return head->value;
    }

    bool empty() {
        return count == 0;
    }

    int size() {
        return count;
    }

    void clear() {
        while (head != nullptr) {
            pop();
        }
    }
};
