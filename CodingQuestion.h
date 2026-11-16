#pragma once
#include <string>
#include "Question.h"
#include "MyStack.h"
using namespace std;

// "Code question": the student writes code for a small problem.
// C++ can't judge free-form code, so the AI grades it against the hidden
// reference solution (see question_ai.py). needsAiGrading() returns true.
class CodingQuestion : public Question {
private:
    string text;
    MyVector<string> starterLines;
    string referenceSolution;

public:
    CodingQuestion(int id, string topic, string subtopic, string difficulty, string text,
                   string starterCode, string referenceSolution, string explanation);

    string getTypeName();
    string getTitle();
    void display(int number, int total);
    string getAnswerPrompt();
    bool isValidAnswer(string answer);
    bool checkAnswer(string answer);
    string getCorrectAnswerText();
    string toJson();
    bool needsAiGrading();
};
