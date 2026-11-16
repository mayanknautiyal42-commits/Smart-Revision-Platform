#include "QuizSession.h"
using namespace std;

QuizSession::QuizSession() {
    serial = 0;
    clear();
}

void QuizSession::clear() {
    queue.clear();
    total = 0;
    number = 0;
    correct = 0;
    attempted = 0;
    currentId = -1;
    mode = "practice";
}

int QuizSession::start(MyVector<int> ids, int count, string mode) {
    clear();
    static int nextSerial = 1;
    serial = nextSerial++;
    this->mode = mode;
    for (int i = 0; i < ids.size() && queue.size() < count; i++) {
        queue.push(ids[i]);
    }
    total = queue.size();
    return total;
}

// Adds questions to the end of a session that is already running: the AI
// writes the first few questions, the session starts, and the rest arrive here.
int QuizSession::append(MyVector<int> ids) {
    for (int i = 0; i < ids.size(); i++) {
        queue.push(ids[i]);
    }
    total += ids.size();
    return ids.size();
}

Question* QuizSession::next(QuestionBank& bank) {
    while (!queue.empty()) {
        int id = queue.front();
        queue.pop();
        Question* q = bank.get(id);
        if (q != nullptr) {
            currentId = id;
            number++;
            return q;
        }
    }
    currentId = -1;
    return nullptr;
}

void QuizSession::recordResult(bool isCorrect) {
    attempted++;
    if (isCorrect) {
        correct++;
    }
    currentId = -1;
}

bool QuizSession::hasCurrent() {
    return currentId >= 0;
}

int QuizSession::getCurrentId() {
    return currentId;
}

int QuizSession::getTotal() {
    return total;
}

int QuizSession::getNumber() {
    return number;
}

int QuizSession::getCorrect() {
    return correct;
}

int QuizSession::getAttempted() {
    return attempted;
}

int QuizSession::getRemaining() {
    return queue.size();
}

string QuizSession::getMode() {
    return mode;
}

int QuizSession::getSerial() {
    return serial;
}
