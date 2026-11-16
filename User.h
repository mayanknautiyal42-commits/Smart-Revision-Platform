#pragma once
#include <string>
using namespace std;

// Every account is identified by its email address (always stored in lower case).
class User {
private:
    string email;
    string name;
    string passwordHash;
    long long createdAt;

public:
    User(string email, string name, string passwordHash, long long createdAt);
    virtual ~User();

    string getEmail();
    string getName();
    string getPasswordHash();
    long long getCreatedAt();
    bool checkPassword(string password);
    void setPassword(string newPassword);
    void setPasswordHash(string hash);

    virtual string getRole() = 0;

    static string hashPassword(string email, string password);
};
