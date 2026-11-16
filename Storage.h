#pragma once
#include <string>
#include "MyStack.h"
using namespace std;

class Storage {
public:
    static void makeFolder(string folder);
    static MyVector<string> readLines(string fileName);
    static bool appendLine(string fileName, string line);
    static bool writeLines(string fileName, MyVector<string> lines);
    static MyVector<string> split(string line, char separator);

    // Text that may contain tabs or new lines (code, answers) is stored on one
    // line: "\" becomes "\\", TAB becomes "\t", new line becomes "\n".
    static string escape(string text);
    static string unescape(string text);
};
