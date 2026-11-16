#pragma once

template <typename T>
class MyVector {
private:
    T* data;
    int count;
    int capacity;

    void grow() {
        int newCapacity = capacity * 2;
        if (newCapacity == 0) {
            newCapacity = 4;
        }
        T* newData = new T[newCapacity];
        for (int i = 0; i < count; i++) {
            newData[i] = data[i];
        }
        delete[] data;
        data = newData;
        capacity = newCapacity;
    }

    void copyFrom(const MyVector& other) {
        count = other.count;
        capacity = other.count;
        data = nullptr;
        if (capacity > 0) {
            data = new T[capacity];
            for (int i = 0; i < count; i++) {
                data[i] = other.data[i];
            }
        }
    }

public:
    MyVector() {
        data = nullptr;
        count = 0;
        capacity = 0;
    }

    MyVector(const MyVector& other) {
        copyFrom(other);
    }

    MyVector& operator=(const MyVector& other) {
        if (this != &other) {
            delete[] data;
            copyFrom(other);
        }
        return *this;
    }

    ~MyVector() {
        delete[] data;
    }

    void push_back(T value) {
        if (count == capacity) {
            grow();
        }
        data[count] = value;
        count++;
    }

    void pop_back() {
        if (count > 0) {
            count--;
        }
    }

    T& operator[](int index) {
        return data[index];
    }

    T& back() {
        return data[count - 1];
    }

    int size() {
        return count;
    }

    bool empty() {
        return count == 0;
    }

    void clear() {
        count = 0;
    }
};

template <typename T>
void insertionSort(MyVector<T>& v) {
    for (int i = 1; i < v.size(); i++) {
        T key = v[i];
        int j = i - 1;
        while (j >= 0 && key < v[j]) {
            v[j + 1] = v[j];
            j--;
        }
        v[j + 1] = key;
    }
}

template <typename T>
class MyStack {
private:
    T* data;
    int topIndex;
    int capacity;

    void grow() {
        int newCapacity = capacity * 2;
        if (newCapacity == 0) {
            newCapacity = 4;
        }
        T* newData = new T[newCapacity];
        for (int i = 0; i <= topIndex; i++) {
            newData[i] = data[i];
        }
        delete[] data;
        data = newData;
        capacity = newCapacity;
    }

    void copyFrom(const MyStack& other) {
        topIndex = other.topIndex;
        capacity = other.capacity;
        data = nullptr;
        if (capacity > 0) {
            data = new T[capacity];
            for (int i = 0; i <= topIndex; i++) {
                data[i] = other.data[i];
            }
        }
    }

public:
    MyStack() {
        data = nullptr;
        topIndex = -1;
        capacity = 0;
    }

    MyStack(const MyStack& other) {
        copyFrom(other);
    }

    MyStack& operator=(const MyStack& other) {
        if (this != &other) {
            delete[] data;
            copyFrom(other);
        }
        return *this;
    }

    ~MyStack() {
        delete[] data;
    }

    bool empty() {
        return topIndex == -1;
    }

    void push(T value) {
        if (topIndex + 1 == capacity) {
            grow();
        }
        topIndex++;
        data[topIndex] = value;
    }

    void pop() {
        if (empty()) {
            return;
        }
        topIndex--;
    }

    T top() {
        return data[topIndex];
    }

    int size() {
        return topIndex + 1;
    }
};
