#include "User.h"
using namespace std;

User::User(string email, string name, string passwordHash, long long createdAt) {
    this->email = email;
    this->name = name;
    this->passwordHash = passwordHash;
    this->createdAt = createdAt;
}

User::~User() {
}

string User::getEmail() {
    return email;
}

string User::getName() {
    return name;
}

string User::getPasswordHash() {
    return passwordHash;
}

long long User::getCreatedAt() {
    return createdAt;
}

bool User::checkPassword(string password) {
    return hashPassword(email, password) == passwordHash;
}

void User::setPassword(string newPassword) {
    passwordHash = hashPassword(email, newPassword);
}

void User::setPasswordHash(string hash) {
    passwordHash = hash;
}

// Salted FNV-1a, repeated 1000 times. Fine for learning; a real system
// should use bcrypt or Argon2 (see README, "Limitations").
string User::hashPassword(string email, string password) {
    unsigned long long hash = 14695981039346656037ULL;
    string text = email + ":" + password;
    for (int round = 0; round < 1000; round++) {
        for (int i = 0; i < (int)text.length(); i++) {
            hash = hash ^ (unsigned char)text[i];
            hash = hash * 1099511628211ULL;
        }
    }
    string hex = "0123456789abcdef";
    string result = "";
    for (int i = 0; i < 16; i++) {
        result = hex[hash % 16] + result;
        hash = hash / 16;
    }
    return result;
}
