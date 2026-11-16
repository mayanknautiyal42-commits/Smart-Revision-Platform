#pragma once
#include <string>
#include "MyStack.h"
using namespace std;

string jsonQuote(string s) {
    string hex = "0123456789abcdef";
    string out = "\"";
    for (int i = 0; i < (int)s.length(); i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '"') {
            out += "\\\"";
        } else if (c == '\\') {
            out += "\\\\";
        } else if (c == '\n') {
            out += "\\n";
        } else if (c == '\r') {
            out += "\\r";
        } else if (c == '\t') {
            out += "\\t";
        } else if (c < 0x20) {
            out += "\\u00";
            out += hex[c >> 4];
            out += hex[c & 15];
        } else {
            out += s[i];
        }
    }
    return out + "\"";
}

class JsonObject {
private:
    string body;

    JsonObject& addRaw(string key, string rawValue) {
        if (body.length() > 0) {
            body += ",";
        }
        body += jsonQuote(key) + ":" + rawValue;
        return *this;
    }

public:
    JsonObject& addStr(string key, string value) {
        return addRaw(key, jsonQuote(value));
    }

    JsonObject& addInt(string key, long long value) {
        return addRaw(key, to_string(value));
    }

    JsonObject& addBool(string key, bool value) {
        if (value) {
            return addRaw(key, "true");
        }
        return addRaw(key, "false");
    }

    JsonObject& addNull(string key) {
        return addRaw(key, "null");
    }

    JsonObject& addJson(string key, string json) {
        return addRaw(key, json);
    }

    string str() {
        return "{" + body + "}";
    }
};

class JsonArray {
private:
    string body;

public:
    JsonArray& addJson(string json) {
        if (body.length() > 0) {
            body += ",";
        }
        body += json;
        return *this;
    }

    JsonArray& addStr(string value) {
        return addJson(jsonQuote(value));
    }

    string str() {
        return "[" + body + "]";
    }

    static string ofStrings(MyVector<string> items) {
        JsonArray a;
        for (int i = 0; i < items.size(); i++) {
            a.addStr(items[i]);
        }
        return a.str();
    }
};
