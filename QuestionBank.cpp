#include "QuestionBank.h"
#include "MCQQuestion.h"
#include "OutputQuestion.h"
#include "CodingQuestion.h"
#include "InputUtils.h"
#include "Storage.h"
#include <cstdlib>
using namespace std;

// Mixes the bits of a number (a small integer hash). Neighbouring inputs
// like 1001 and 1002 give unrelated outputs.
unsigned int mixBits(unsigned int x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

// Fisher-Yates shuffle seeded by the question id. AI models tend to put the
// right answer in the same position, so the options are shuffled once when
// the question is saved. Each swap uses a fresh hash of (id, step): AI
// questions get consecutive ids, and a plain LCG seeded with consecutive
// ids put the answer in some positions far more often than others.
void shuffleOptions(MyVector<string>& options, int& correctIndex, int seed) {
    for (int i = options.size() - 1; i > 0; i--) {
        int j = (int)(mixBits((unsigned int)seed * 8u + (unsigned int)i) % (unsigned int)(i + 1));
        string temp = options[i];
        options[i] = options[j];
        options[j] = temp;
        if (correctIndex == i) {
            correctIndex = j;
        } else if (correctIndex == j) {
            correctIndex = i;
        }
    }
}

QuestionBank::QuestionBank() {
    nextId = 1001;
    fileName = "";
}

QuestionBank::~QuestionBank() {
    for (int i = 0; i < order.size(); i++) {
        delete questions.get(order[i]);
    }
}

void QuestionBank::setFile(string fileName) {
    this->fileName = fileName;
}

Question* QuestionBank::fromRecord(MyVector<string> f) {
    int id;
    if (f.size() < R_FIELD_COUNT || !parseInt(f[R_ID], id)) {
        return nullptr;
    }
    string type = f[R_TYPE];
    if (type == "MCQ") {
        int answer;
        if (!parseInt(f[R_ANSWER], answer) || answer < 1 || answer > 4) {
            return nullptr;
        }
        MyVector<string> options;
        for (int k = R_OPT1; k <= R_OPT4; k++) {
            options.push_back(f[k]);
        }
        return new MCQQuestion(id, f[R_TOPIC], f[R_SUBTOPIC], f[R_LEVEL], f[R_TEXT], f[R_CODE],
                               options, answer - 1, f[R_EXPLANATION]);
    }
    if (type == "Output") {
        return new OutputQuestion(id, f[R_TOPIC], f[R_SUBTOPIC], f[R_LEVEL], f[R_TEXT], f[R_CODE],
                                  f[R_ANSWER], f[R_EXPLANATION]);
    }
    if (type == "Code") {
        return new CodingQuestion(id, f[R_TOPIC], f[R_SUBTOPIC], f[R_LEVEL], f[R_TEXT], f[R_CODE],
                                  f[R_ANSWER], f[R_EXPLANATION]);
    }
    return nullptr;
}

// Reads data/questions.txt. Returns how many questions were loaded.
int QuestionBank::load() {
    MyVector<string> lines = Storage::readLines(fileName);
    int loaded = 0;
    for (int i = 0; i < lines.size(); i++) {
        MyVector<string> raw = Storage::split(lines[i], '\t');
        MyVector<string> f;
        for (int k = 0; k < raw.size(); k++) {
            f.push_back(Storage::unescape(raw[k]));
        }
        Question* q = fromRecord(f);
        if (q != nullptr && isDatasetId(q->getId())) {
            delete q;   // question-set ids belong to dataset/questions_dataset.txt only
            continue;
        }
        if (q != nullptr && add(q)) {
            createdAt.insert(q->getId(), atoll(f[R_CREATED].c_str()));
            loaded++;
        }
    }
    return loaded;
}

// Adds a new question. "fields" has every record field except id and created
// (12 values, starting with type). Returns the new id, or -1 with an error.
int QuestionBank::addRecord(MyVector<string> fields, long long now, string& error) {
    if (fields.size() != R_FIELD_COUNT - 2) {
        error = "A question needs 12 fields.";
        return -1;
    }
    if (nextId >= DATASET_FIRST_ID) {
        error = "No free question ids are left for AI questions.";
        return -1;
    }
    int id = nextId;
    MyVector<string> f;
    f.push_back(to_string(id));
    for (int k = 0; k < fields.size(); k++) {
        f.push_back(fields[k]);
        if (k == R_LEVEL - 1) {
            f.push_back(to_string(now));
        }
    }

    if (f[R_TYPE] == "MCQ") {
        int answer;
        if (!parseInt(f[R_ANSWER], answer) || answer < 1 || answer > 4) {
            error = "MCQ answer must be 1 to 4.";
            return -1;
        }
        MyVector<string> options;
        for (int k = R_OPT1; k <= R_OPT4; k++) {
            options.push_back(f[k]);
        }
        int correct = answer - 1;
        shuffleOptions(options, correct, id);
        for (int k = 0; k < 4; k++) {
            f[R_OPT1 + k] = options[k];
        }
        f[R_ANSWER] = to_string(correct + 1);
    }

    Question* q = fromRecord(f);
    if (q == nullptr) {
        error = "The question data is not valid.";
        return -1;
    }
    if (fileName != "") {
        string line = "";
        for (int k = 0; k < f.size(); k++) {
            if (k > 0) {
                line += "\t";
            }
            line += Storage::escape(f[k]);
        }
        if (!Storage::appendLine(fileName, line)) {
            delete q;
            error = "Could not save the question to the data folder.";
            return -1;
        }
    }
    add(q);
    createdAt.insert(id, now);
    return id;
}

// Adds one question from the question set (dataset/questions_dataset.txt).
// The file is written by import_dataset.py and is never changed by C++.
// MCQ options are shuffled with the id as the seed, so the order is the same
// on every start: an option number saved in attempts.txt keeps its meaning.
bool QuestionBank::addDatasetRecord(MyVector<string> f, string& error) {
    int id;
    if (f.size() != R_FIELD_COUNT || !parseInt(f[R_ID], id) || !isDatasetId(id)) {
        error = "id must be " + to_string(DATASET_FIRST_ID + 1) + " to " + to_string(DATASET_LAST_ID) + ".";
        return false;
    }
    if (questions.contains(id)) {
        error = "id " + to_string(id - DATASET_FIRST_ID) + " is used twice.";
        return false;
    }
    if (f[R_TYPE] == "MCQ") {
        int answer;
        if (!parseInt(f[R_ANSWER], answer) || answer < 1 || answer > 4) {
            error = "MCQ answer must be 1 to 4.";
            return false;
        }
        MyVector<string> options;
        for (int k = R_OPT1; k <= R_OPT4; k++) {
            options.push_back(f[k]);
        }
        int correct = answer - 1;
        shuffleOptions(options, correct, id);
        for (int k = 0; k < 4; k++) {
            f[R_OPT1 + k] = options[k];
        }
        f[R_ANSWER] = to_string(correct + 1);
    }
    Question* q = fromRecord(f);
    if (q == nullptr) {
        error = "The question data is not valid.";
        return false;
    }
    add(q);
    createdAt.insert(id, atoll(f[R_CREATED].c_str()));
    return true;
}

bool QuestionBank::isDatasetId(int id) {
    return id > DATASET_FIRST_ID && id <= DATASET_LAST_ID;
}

int QuestionBank::countDataset() {
    return getDatasetIds().size();
}

MyVector<int> QuestionBank::getDatasetIds() {
    MyVector<int> result;
    for (int i = 0; i < order.size(); i++) {
        if (isDatasetId(order[i])) {
            result.push_back(order[i]);
        }
    }
    return result;
}

bool QuestionBank::add(Question* q) {
    if (q == nullptr) {
        return false;
    }
    if (questions.contains(q->getId())) {
        delete q;
        return false;
    }
    questions.insert(q->getId(), q);
    order.push_back(q->getId());
    if (!isDatasetId(q->getId()) && q->getId() >= nextId) {
        nextId = q->getId() + 1;
    }
    return true;
}

Question* QuestionBank::get(int id) {
    if (!questions.contains(id)) {
        return nullptr;
    }
    return questions.get(id);
}

int QuestionBank::size() {
    return order.size();
}

MyVector<int> QuestionBank::getIds() {
    return order;
}

long long QuestionBank::getCreatedAt(int id) {
    if (!createdAt.contains(id)) {
        return 0;
    }
    return createdAt.get(id);
}

bool QuestionBank::matches(Question* q, string topic, string level, string type) {
    if (topic != "" && q->getTopic() != topic) {
        return false;
    }
    if (level != "" && q->getDifficulty() != level) {
        return false;
    }
    if (type != "" && q->getTypeName() != type) {
        return false;
    }
    return true;
}

MyVector<int> QuestionBank::filter(string topic, string level, string type) {
    MyVector<int> result;
    for (int i = 0; i < order.size(); i++) {
        Question* q = questions.get(order[i]);
        if (matches(q, topic, level, type)) {
            result.push_back(order[i]);
        }
    }
    return result;
}

int QuestionBank::count(string topic, string level, string type) {
    return filter(topic, level, type).size();
}

MyVector<int> QuestionBank::filterDataset(string topic, string subtopic, string level, string type) {
    MyVector<int> result;
    for (int i = 0; i < order.size(); i++) {
        if (!isDatasetId(order[i])) {
            continue;
        }
        Question* q = questions.get(order[i]);
        if (matches(q, topic, level, type) && (subtopic == "" || q->getSubtopic() == subtopic)) {
            result.push_back(order[i]);
        }
    }
    return result;
}
