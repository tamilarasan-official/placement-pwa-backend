#pragma once

#include <string>
#include <json/json.h>

class JwtHelper {
public:
    static std::string generateToken(const std::string& userId, const std::string& role);
    static Json::Value verifyToken(const std::string& token);
    static std::string getSecret();
};
