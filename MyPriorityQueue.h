#pragma once
#include "MyStack.h"

template <typename T>
class MyPriorityQueue {
private:
    MyVector<T> heap;

    void swapItems(int a, int b) {
        T temp = heap[a];
        heap[a] = heap[b];
        heap[b] = temp;
    }

    void siftUp(int i) {
        while (i > 0) {
            int parent = (i - 1) / 2;
            if (heap[i] < heap[parent]) {
                swapItems(i, parent);
                i = parent;
            } else {
                break;
            }
        }
    }

    void siftDown(int i) {
        int n = heap.size();
        while (true) {
            int left = 2 * i + 1;
            int right = 2 * i + 2;
            int smallest = i;
            if (left < n && heap[left] < heap[smallest]) {
                smallest = left;
            }
            if (right < n && heap[right] < heap[smallest]) {
                smallest = right;
            }
            if (smallest == i) {
                break;
            }
            swapItems(i, smallest);
            i = smallest;
        }
    }

public:
    void push(T value) {
        heap.push_back(value);
        siftUp(heap.size() - 1);
    }

    T top() {
        return heap[0];
    }

    void pop() {
        if (heap.empty()) {
            return;
        }
        heap[0] = heap.back();
        heap.pop_back();
        if (!heap.empty()) {
            siftDown(0);
        }
    }

    bool empty() {
        return heap.empty();
    }

    int size() {
        return heap.size();
    }
};
