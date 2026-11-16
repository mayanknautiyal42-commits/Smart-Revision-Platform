#include "MCQQuestion.h"
#include "InputUtils.h"
#include <iostream>
using namespace std;

MCQQuestion::MCQQuestion(int id, string topic, string subtopic, string difficulty, string text, string code,
                         MyVector<string> options, int correctIndex, string explanation)
    : Question(id, topic, subtopic, difficulty, explanation) {
    this->text = text;
    this->codeLines = splitCodeLines(code);
    this->options = options;
    this->correctIndex = correctIndex;
}

string MCQQuestion::getTypeName() {
    return "MCQ";
}

string MCQQuestion::getTitle() {
    return text;
}

void MCQQuestion::display(int number, int total) {
    displayHeader(number, total);
    cout << text << "\n\n";
    for (int i = 0; i < codeLines.size(); i++) {
        cout << "    " << codeLines[i] << "\n";
    }
    if (codeLines.size() > 0) {
        cout << "\n";
    }
    for (int i = 0; i < options.size(); i++) {
        cout << "  [" << (i + 1) << "] " << options[i] << "\n";
    }
}

string MCQQuestion::getAnswerPrompt() {
    return "Your answer (1-" + to_string(options.size()) + ", q = quit):";
}

bool MCQQuestion::isValidAnswer(string answer) {
    int choice;
    if (!parseInt(answer, choice)) {
        return false;
    }
    return choice >= 1 && choice <= options.size();
}

bool MCQQuestion::checkAnswer(string answer) {
    int choice;
    if (!parseInt(answer, choice)) {
        return false;
    }
    return choice - 1 == correctIndex;
}

string MCQQuestion::getCorrectAnswerText() {
    return to_string(correctIndex + 1) + ". " + options[correctIndex];
}

string MCQQuestion::describeAnswer(string answer) {
    if (!isValidAnswer(answer)) {
        return answer;
    }
    int choice;
    parseInt(answer, choice);
    return to_string(choice) + ". " + options[choice - 1];
}

string MCQQuestion::toJson() {
    JsonObject obj;
    addCommonJson(obj);
    obj.addStr("text", text)
       .addJson("code", JsonArray::ofStrings(codeLines))
       .addJson("options", JsonArray::ofStrings(options));
    return obj.str();
}
