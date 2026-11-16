#pragma once
#include "User.h"

class Admin : public User {
public:
    Admin(string email, string name, string passwordHash, long long createdAt)
        : User(email, name, passwordHash, createdAt) {
    }

    string getRole() {
        return "admin";
    }
};
