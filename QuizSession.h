#pragma once
#include <string>
#include "MyQueue.h"
#include "MyStack.h"
#include "QuestionBank.h"
using namespace std;

struct AnswerOutcome {
    int questionId;
    bool correct;
    string correctAnswer;
    string explanation;
    string feedback;   // AI feedback for code questions, empty otherwise
    string myAnswer;   // the student's answer as it is shown back
};

class QuizSession {
private:
    MyQueue<int> queue;
    int total;
    int number;
    int correct;
    int attempted;
    int currentId;
    int serial;      // changes with every start(), so late additions can't reach a newer session
    string mode;

public:
    QuizSession();

    int start(MyVector<int> ids, int count, string mode);
    int append(MyVector<int> ids);
    Question* next(QuestionBank& bank);
    void recordResult(bool isCorrect);
    void clear();

    bool hasCurrent();
    int getCurrentId();
    int getTotal();
    int getNumber();
    int getCorrect();
    int getAttempted();
    int getRemaining();
    string getMode();
    int getSerial();
};
