#pragma once
#include <string>
#include "Question.h"
#include "MyStack.h"
using namespace std;

class MCQQuestion : public Question {
private:
    string text;
    MyVector<string> codeLines;
    MyVector<string> options;
    int correctIndex;

public:
    MCQQuestion(int id, string topic, string subtopic, string difficulty, string text, string code,
                MyVector<string> options, int correctIndex, string explanation);

    string getTypeName();
    string getTitle();
    void display(int number, int total);
    string getAnswerPrompt();
    bool isValidAnswer(string answer);
    bool checkAnswer(string answer);
    string getCorrectAnswerText();
    string toJson();
    string describeAnswer(string answer);
};
