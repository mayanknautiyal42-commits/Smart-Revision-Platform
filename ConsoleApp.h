#pragma once
#include <string>
#include "Platform.h"
#include "QuizSession.h"
using namespace std;

class ConsoleApp {
private:
    Platform& platform;
    QuizSession session;

    bool askChoice(string label, int minVal, int maxVal, int& out);
    bool askText(string label, string& out);
    string percentText(double p);

    bool setupAdmin();
    bool registerStudent();
    bool loginUser();

    bool studentMenu(Student* s);
    bool practise(Student* s);
    MyVector<int> withoutCodeQuestions(MyVector<int> ids);
    bool reviewMistakes(Student* s);
    bool runSession(Student* s);
    void showReport(Student* s);
    void showPlan(Student* s);
    void showRecent(Student* s);

    bool adminMenu();
    void showStudents();
    bool showStudentDetails();
    void showMostMissed();

public:
    ConsoleApp(Platform& platform);
    void run();
};
