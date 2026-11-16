#pragma once
#include "MyHashMap.h"

template <typename K>
class MyHashSet {
private:
    MyHashMap<K, bool> table;

public:
    void insert(K key) {
        table.insert(key, true);
    }

    bool contains(K key) {
        return table.contains(key);
    }

    int size() {
        return table.size();
    }

    bool empty() {
        return table.empty();
    }
};
