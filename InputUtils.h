#pragma once
#include <iostream>
#include <string>
using namespace std;

bool readLine(string& out) {
    if (!getline(cin, out)) {
        return false;
    }
    if (out.length() > 0 && out[out.length() - 1] == '\r') {
        out.erase(out.length() - 1);
    }
    return true;
}

bool isSpaceChar(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

string trim(string s) {
    int start = 0;
    int end = (int)s.length() - 1;
    while (start <= end && isSpaceChar(s[start])) {
        start++;
    }
    while (end >= start && isSpaceChar(s[end])) {
        end--;
    }
    return s.substr(start, end - start + 1);
}

string normalizeSpaces(string s) {
    string result = "";
    bool pendingSpace = false;
    for (int i = 0; i < (int)s.length(); i++) {
        if (isSpaceChar(s[i])) {
            pendingSpace = true;
        } else {
            if (pendingSpace && result.length() > 0) {
                result += ' ';
            }
            result += s[i];
            pendingSpace = false;
        }
    }
    return result;
}

bool parseInt(string text, int& out) {
    string s = trim(text);
    if (s.length() == 0 || s.length() > 9) {
        return false;
    }
    int value = 0;
    for (int i = 0; i < (int)s.length(); i++) {
        if (s[i] < '0' || s[i] > '9') {
            return false;
        }
        value = value * 10 + (s[i] - '0');
    }
    out = value;
    return true;
}

string toLowerCase(string s) {
    for (int i = 0; i < (int)s.length(); i++) {
        if (s[i] >= 'A' && s[i] <= 'Z') {
            s[i] = s[i] - 'A' + 'a';
        }
    }
    return s;
}

bool hasTabOrNewline(string s) {
    for (int i = 0; i < (int)s.length(); i++) {
        if (s[i] == '\t' || s[i] == '\n' || s[i] == '\r') {
            return true;
        }
    }
    return false;
}
