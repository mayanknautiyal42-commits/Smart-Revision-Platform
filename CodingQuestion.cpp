#include "CodingQuestion.h"
#include "InputUtils.h"
#include <iostream>
using namespace std;

CodingQuestion::CodingQuestion(int id, string topic, string subtopic, string difficulty, string text,
                               string starterCode, string referenceSolution, string explanation)
    : Question(id, topic, subtopic, difficulty, explanation) {
    this->text = text;
    this->starterLines = splitCodeLines(starterCode);
    this->referenceSolution = referenceSolution;
}

string CodingQuestion::getTypeName() {
    return "Code";
}

string CodingQuestion::getTitle() {
    string line = text;
    size_t newline = line.find('\n');
    if (newline != string::npos) {
        line = line.substr(0, newline);
    }
    if (line.length() > 90) {
        line = line.substr(0, 90) + "...";
    }
    return line;
}

void CodingQuestion::display(int number, int total) {
    displayHeader(number, total);
    cout << text << "\n\n";
    for (int i = 0; i < starterLines.size(); i++) {
        cout << "    " << starterLines[i] << "\n";
    }
}

string CodingQuestion::getAnswerPrompt() {
    return "Write your code on the website (AI grading needs the web version).";
}

bool CodingQuestion::isValidAnswer(string answer) {
    return trim(answer) != "";
}

bool CodingQuestion::checkAnswer(string answer) {
    (void)answer;   // graded by the AI instead
    return false;
}

string CodingQuestion::getCorrectAnswerText() {
    return referenceSolution;
}

string CodingQuestion::toJson() {
    JsonObject obj;
    addCommonJson(obj);
    obj.addStr("text", text).addJson("code", JsonArray::ofStrings(starterLines));
    return obj.str();
}

bool CodingQuestion::needsAiGrading() {
    return true;
}
