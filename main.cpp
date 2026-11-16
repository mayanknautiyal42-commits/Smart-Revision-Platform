#include <iostream>
#include <string>
#include "Platform.h"
#include "ApiServer.h"
#include "ConsoleApp.h"
#include "Json.h"
#include "Storage.cpp"
#include "User.cpp"
#include "Student.cpp"
#include "Question.cpp"
#include "MCQQuestion.cpp"
#include "OutputQuestion.cpp"
#include "CodingQuestion.cpp"
#include "QuestionBank.cpp"
#include "Syllabus.cpp"
#include "QuizSession.cpp"
#include "RevisionManager.cpp"
#include "Platform.cpp"
#include "ApiServer.cpp"
#include "ConsoleApp.cpp"
using namespace std;

// smart_revision                          terminal app
// smart_revision --api                    JSON API for web_app.py
// smart_revision --api <folder> [<file>]  same, but keep the data in <folder> and read the
//                                         question set from <file> (used by test_web.py)
// smart_revision --syllabus               prints the syllabus as JSON (used by import_dataset.py,
//                                         so the topic names are written in one place only)
int main(int argc, char* argv[]) {
    if (argc > 1 && string(argv[1]) == "--syllabus") {
        Syllabus syllabus;
        loadSyllabus(syllabus);
        MyVector<string> topics = syllabus.getTopics();
        JsonArray list;
        for (int i = 0; i < topics.size(); i++) {
            list.addJson(JsonObject().addStr("name", topics[i])
                .addJson("subtopics", JsonArray::ofStrings(syllabus.getSubtopics(topics[i]))).str());
        }
        cout << list.str() << "\n";
        return 0;
    }
    string dataFolder = "data";
    string datasetFile = "dataset/questions_dataset.txt";
    if (argc > 2) {
        dataFolder = argv[2];
    }
    if (argc > 3) {
        datasetFile = argv[3];
    }
    Platform platform(dataFolder, datasetFile);
    platform.init();

    if (argc > 1 && string(argv[1]) == "--api") {
        ApiServer server(platform);
        server.run();
        return 0;
    }

    ConsoleApp app(platform);
    app.run();
    return 0;
}
