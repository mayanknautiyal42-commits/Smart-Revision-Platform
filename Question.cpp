#include "Question.h"
#include "InputUtils.h"
#include <iostream>
using namespace std;

MyVector<string> splitCodeLines(string code) {
    MyVector<string> lines;
    string current = "";
    for (int i = 0; i < (int)code.length(); i++) {
        if (code[i] == '\n') {
            lines.push_back(current);
            current = "";
        } else if (code[i] != '\r') {
            current += code[i];
        }
    }
    lines.push_back(current);

    int first = 0;
    int last = lines.size() - 1;
    while (first <= last && trim(lines[first]) == "") {
        first++;
    }
    while (last >= first && trim(lines[last]) == "") {
        last--;
    }
    MyVector<string> result;
    for (int i = first; i <= last; i++) {
        result.push_back(lines[i]);
    }
    return result;
}

Question::Question(int id, string topic, string subtopic, string difficulty, string explanation) {
    this->id = id;
    this->topic = topic;
    this->subtopic = subtopic;
    this->difficulty = difficulty;
    this->explanation = explanation;
}

Question::~Question() {
}

int Question::getId() {
    return id;
}

string Question::getTopic() {
    return topic;
}

string Question::getSubtopic() {
    return subtopic;
}

string Question::getDifficulty() {
    return difficulty;
}

string Question::getExplanation() {
    return explanation;
}

string Question::describeAnswer(string answer) {
    return answer;
}

bool Question::needsAiGrading() {
    return false;
}

void Question::displayHeader(int number, int total) {
    cout << "\n----------------------------------------------------------------\n";
    cout << "Question " << number << " of " << total
         << "   [" << topic << " > " << subtopic << " | " << difficulty << " | " << getTypeName() << "]\n";
    cout << "----------------------------------------------------------------\n";
}

void Question::addCommonJson(JsonObject& obj) {
    obj.addInt("id", id)
       .addStr("topic", topic)
       .addStr("subtopic", subtopic)
       .addStr("difficulty", difficulty)
       .addStr("type", getTypeName());
}
