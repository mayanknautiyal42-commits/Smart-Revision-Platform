#include "OutputQuestion.h"
#include "InputUtils.h"
#include <iostream>
using namespace std;

OutputQuestion::OutputQuestion(int id, string topic, string subtopic, string difficulty, string text,
                               string code, string expectedOutput, string explanation)
    : Question(id, topic, subtopic, difficulty, explanation) {
    this->prompt = trim(text) == "" ? "What does this code print?" : text;
    this->codeLines = splitCodeLines(code);
    this->expectedOutput = expectedOutput;
}

string OutputQuestion::getTypeName() {
    return "Output";
}

string OutputQuestion::getTitle() {
    for (int i = 0; i < codeLines.size(); i++) {
        string line = trim(codeLines[i]);
        if (line == "" || line[0] == '#' || line.find("using namespace") == 0 || line.find("int main") == 0 ||
            line == "{" || line == "}" || line.find("//") == 0) {
            continue;
        }
        if (line.length() > 40) {
            line = line.substr(0, 40) + "...";
        }
        return "Predict the output: " + line;
    }
    return prompt;
}

void OutputQuestion::display(int number, int total) {
    displayHeader(number, total);
    cout << prompt << "\n\n";
    for (int i = 0; i < codeLines.size(); i++) {
        string lineNo = to_string(i + 1);
        if (lineNo.length() < 2) {
            lineNo = " " + lineNo;
        }
        cout << "  " << lineNo << " | " << codeLines[i] << "\n";
    }
}

string OutputQuestion::getAnswerPrompt() {
    return "Type the exact output (q = quit):";
}

bool OutputQuestion::isValidAnswer(string answer) {
    return trim(answer) != "";
}

bool OutputQuestion::checkAnswer(string answer) {
    return normalizeSpaces(answer) == normalizeSpaces(expectedOutput);
}

string OutputQuestion::getCorrectAnswerText() {
    return expectedOutput;
}

string OutputQuestion::toJson() {
    JsonObject obj;
    addCommonJson(obj);
    obj.addStr("text", prompt).addJson("code", JsonArray::ofStrings(codeLines));
    return obj.str();
}
