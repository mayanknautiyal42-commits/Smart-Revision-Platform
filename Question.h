#pragma once
#include <string>
#include "Json.h"
#include "MyStack.h"
using namespace std;

// Splits code text into lines and drops blank lines at the start and end.
MyVector<string> splitCodeLines(string code);

class Question {
private:
    int id;
    string topic;
    string subtopic;
    string difficulty;
    string explanation;

protected:
    void displayHeader(int number, int total);
    void addCommonJson(JsonObject& obj);

public:
    Question(int id, string topic, string subtopic, string difficulty, string explanation);
    virtual ~Question();

    int getId();
    string getTopic();
    string getSubtopic();
    string getDifficulty();
    string getExplanation();

    // "MCQ", "Output" (predict the output) or "Code" (write code)
    virtual string getTypeName() = 0;
    virtual string getTitle() = 0;
    virtual void display(int number, int total) = 0;
    virtual string getAnswerPrompt() = 0;
    virtual bool isValidAnswer(string answer) = 0;
    virtual bool checkAnswer(string answer) = 0;
    virtual string getCorrectAnswerText() = 0;
    virtual string toJson() = 0;

    // How the student's answer is shown back to them ("2. O(n)" for an MCQ).
    virtual string describeAnswer(string answer);
    // True when C++ cannot check the answer itself and the AI must grade it.
    virtual bool needsAiGrading();
};
