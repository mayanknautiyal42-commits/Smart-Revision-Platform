#pragma once
#include <string>
#include "MyHashMap.h"
#include "MyStack.h"
#include "Question.h"
using namespace std;

// One saved question = one line of data/questions.txt (AI questions) or of
// dataset/questions_dataset.txt (the team's question set) with these 14 fields
// (text fields are escaped with Storage::escape):
//   id  type  topic  subtopic  difficulty  created  text  code  opt1  opt2  opt3  opt4  answer  explanation
// "answer" is the option number (1-4) for an MCQ, the exact output for an
// Output question, and the reference solution for a Code question.
enum RecordField {
    R_ID, R_TYPE, R_TOPIC, R_SUBTOPIC, R_LEVEL, R_CREATED, R_TEXT, R_CODE,
    R_OPT1, R_OPT2, R_OPT3, R_OPT4, R_ANSWER, R_EXPLANATION, R_FIELD_COUNT
};

unsigned int mixBits(unsigned int x);
void shuffleOptions(MyVector<string>& options, int& correctIndex, int seed);

// Question ids: AI questions count up from 1001; question-set (dataset)
// questions use DATASET_FIRST_ID + the id written in the CSV (1 to 99999).
// Keeping the two ranges apart means an id in attempts.txt always points to
// the same question, even after the question set is edited and reloaded.
const int DATASET_FIRST_ID = 500000;
const int DATASET_LAST_ID = DATASET_FIRST_ID + 99999;

class QuestionBank {
private:
    MyHashMap<int, Question*> questions;
    MyHashMap<int, long long> createdAt;
    MyVector<int> order;
    int nextId;
    string fileName;

    Question* fromRecord(MyVector<string> f);

public:
    QuestionBank();
    ~QuestionBank();

    void setFile(string fileName);
    int load();
    int addRecord(MyVector<string> fields, long long now, string& error);
    // A question-set record: all 14 fields, id already in the dataset range.
    bool addDatasetRecord(MyVector<string> f, string& error);

    static bool isDatasetId(int id);
    int countDataset();
    MyVector<int> getDatasetIds();

    bool add(Question* q);
    Question* get(int id);
    int size();
    MyVector<int> getIds();
    long long getCreatedAt(int id);

    bool matches(Question* q, string topic, string level, string type);
    MyVector<int> filter(string topic, string level, string type);
    // Question-set ids for a choice; "" means any (subtopic too).
    MyVector<int> filterDataset(string topic, string subtopic, string level, string type);
    int count(string topic, string level, string type);
};
