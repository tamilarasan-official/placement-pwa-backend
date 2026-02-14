#pragma once

#include <json/json.h>
#include <string>

class AuthService {
public:
    static Json::Value registerUser(const Json::Value& body);
    static Json::Value loginUser(const Json::Value& body);
    static Json::Value getUserById(const std::string& userId);
};
