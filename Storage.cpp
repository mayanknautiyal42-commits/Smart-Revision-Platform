#include "Storage.h"
#include <cstdio>
#include <fstream>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif
using namespace std;

void Storage::makeFolder(string folder) {
#ifdef _WIN32
    _mkdir(folder.c_str());
#else
    mkdir(folder.c_str(), 0755);
#endif
}

MyVector<string> Storage::readLines(string fileName) {
    MyVector<string> lines;
    ifstream file(fileName);
    string line;
    while (getline(file, line)) {
        if (line.length() > 0 && line[line.length() - 1] == '\r') {
            line.erase(line.length() - 1);
        }
        if (line.length() > 0) {
            lines.push_back(line);
        }
    }
    return lines;
}

bool Storage::appendLine(string fileName, string line) {
    ofstream file(fileName, ios::app);
    if (!file) {
        return false;
    }
    file << line << "\n";
    return true;
}

// Writes the whole file again (used when a password changes). The new text
// goes to a temporary file first, so a crash never leaves a half-written file.
bool Storage::writeLines(string fileName, MyVector<string> lines) {
    string temp = fileName + ".tmp";
    {
        ofstream file(temp, ios::trunc);
        if (!file) {
            return false;
        }
        for (int i = 0; i < lines.size(); i++) {
            file << lines[i] << "\n";
        }
        if (!file) {
            return false;
        }
    }
    remove(fileName.c_str());
    return rename(temp.c_str(), fileName.c_str()) == 0;
}

MyVector<string> Storage::split(string line, char separator) {
    MyVector<string> parts;
    string current = "";
    for (int i = 0; i < (int)line.length(); i++) {
        if (line[i] == separator) {
            parts.push_back(current);
            current = "";
        } else {
            current += line[i];
        }
    }
    parts.push_back(current);
    return parts;
}

string Storage::escape(string text) {
    string out = "";
    for (int i = 0; i < (int)text.length(); i++) {
        char c = text[i];
        if (c == '\\') out += "\\\\";
        else if (c == '\t') out += "\\t";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else out += c;
    }
    return out;
}

string Storage::unescape(string text) {
    string out = "";
    for (int i = 0; i < (int)text.length(); i++) {
        if (text[i] == '\\' && i + 1 < (int)text.length()) {
            char n = text[i + 1];
            if (n == 't') { out += '\t'; i++; continue; }
            if (n == 'n') { out += '\n'; i++; continue; }
            if (n == 'r') { out += '\r'; i++; continue; }
            if (n == '\\') { out += '\\'; i++; continue; }
        }
        out += text[i];
    }
    return out;
}
