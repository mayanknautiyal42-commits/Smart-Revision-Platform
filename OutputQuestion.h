#pragma once
#include <string>
#include "Question.h"
#include "MyStack.h"
using namespace std;

// "Predict the output": the student reads a C++ program and types what it prints.
class OutputQuestion : public Question {
private:
    string prompt;
    MyVector<string> codeLines;
    string expectedOutput;

public:
    OutputQuestion(int id, string topic, string subtopic, string difficulty, string text,
                   string code, string expectedOutput, string explanation);

    string getTypeName();
    string getTitle();
    void display(int number, int total);
    string getAnswerPrompt();
    bool isValidAnswer(string answer);
    bool checkAnswer(string answer);
    string getCorrectAnswerText();
    string toJson();
};
