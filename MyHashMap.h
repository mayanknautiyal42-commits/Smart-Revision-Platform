#pragma once
#include <string>
#include "MyStack.h"
using namespace std;

int hashKey(int key, int size) {
    if (key < 0) {
        key = -key;
    }
    return key % size;
}

int hashKey(string key, int size) {
    int hash = 0;
    for (int i = 0; i < (int)key.length(); i++) {
        hash = (hash * 31 + (unsigned char)key[i]) % size;
    }
    return hash;
}

template <typename K, typename V>
class MyHashMap {
private:
    struct Node {
        K key;
        V value;
        Node* next;
    };

    Node** buckets;
    int bucketCount;
    int count;

    void createBuckets(int n) {
        bucketCount = n;
        buckets = new Node*[n];
        for (int i = 0; i < n; i++) {
            buckets[i] = nullptr;
        }
    }

    void deleteAll() {
        for (int i = 0; i < bucketCount; i++) {
            Node* current = buckets[i];
            while (current != nullptr) {
                Node* next = current->next;
                delete current;
                current = next;
            }
        }
        delete[] buckets;
    }

    void rehash() {
        Node** oldBuckets = buckets;
        int oldCount = bucketCount;
        createBuckets(oldCount * 2);
        for (int i = 0; i < oldCount; i++) {
            Node* current = oldBuckets[i];
            while (current != nullptr) {
                Node* next = current->next;
                int index = hashKey(current->key, bucketCount);
                current->next = buckets[index];
                buckets[index] = current;
                current = next;
            }
        }
        delete[] oldBuckets;
    }

    Node* findNode(K key) {
        int index = hashKey(key, bucketCount);
        Node* current = buckets[index];
        while (current != nullptr) {
            if (current->key == key) {
                return current;
            }
            current = current->next;
        }
        return nullptr;
    }

    void copyFrom(const MyHashMap& other) {
        createBuckets(other.bucketCount);
        count = 0;
        for (int i = 0; i < other.bucketCount; i++) {
            Node* current = other.buckets[i];
            while (current != nullptr) {
                insert(current->key, current->value);
                current = current->next;
            }
        }
    }

public:
    MyHashMap() {
        createBuckets(16);
        count = 0;
    }

    MyHashMap(const MyHashMap& other) {
        copyFrom(other);
    }

    MyHashMap& operator=(const MyHashMap& other) {
        if (this != &other) {
            deleteAll();
            copyFrom(other);
        }
        return *this;
    }

    ~MyHashMap() {
        deleteAll();
    }

    void insert(K key, V value) {
        Node* existing = findNode(key);
        if (existing != nullptr) {
            existing->value = value;
            return;
        }
        if ((count + 1) * 4 > bucketCount * 3) {
            rehash();
        }
        int index = hashKey(key, bucketCount);
        Node* node = new Node;
        node->key = key;
        node->value = value;
        node->next = buckets[index];
        buckets[index] = node;
        count++;
    }

    bool contains(K key) {
        return findNode(key) != nullptr;
    }

    V& get(K key) {
        Node* node = findNode(key);
        if (node == nullptr) {
            insert(key, V());
            node = findNode(key);
        }
        return node->value;
    }

    int size() {
        return count;
    }

    bool empty() {
        return count == 0;
    }

    int getBucketCount() {
        return bucketCount;
    }

    MyVector<K> keys() {
        MyVector<K> result;
        for (int i = 0; i < bucketCount; i++) {
            Node* current = buckets[i];
            while (current != nullptr) {
                result.push_back(current->key);
                current = current->next;
            }
        }
        return result;
    }
};
