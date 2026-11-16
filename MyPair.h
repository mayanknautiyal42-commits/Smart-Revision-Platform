#pragma once

template <typename A, typename B>
struct MyPair {
    A first;
    B second;

    MyPair() {
        first = A();
        second = B();
    }

    MyPair(A a, B b) {
        first = a;
        second = b;
    }
};

template <typename A, typename B>
bool operator<(const MyPair<A, B>& x, const MyPair<A, B>& y) {
    if (x.first < y.first) {
        return true;
    }
    if (y.first < x.first) {
        return false;
    }
    return x.second < y.second;
}
