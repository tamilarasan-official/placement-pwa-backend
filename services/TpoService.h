#pragma once

#include <json/json.h>
#include <string>

class TpoService {
public:
    static Json::Value getPendingStudents();
    static Json::Value approveStudent(const std::string& userId);
    static Json::Value rejectStudent(const std::string& userId);
    static Json::Value createRecruiterAccount(const Json::Value& body, const std::string& tpoId);
    static Json::Value getAllRecruiters();
};
