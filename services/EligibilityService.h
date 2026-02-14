#pragma once

#include <json/json.h>
#include <string>

class EligibilityService {
public:
    static Json::Value getEligibleDrives(const std::string& studentId);
    static Json::Value getRecommendedDrives(const std::string& studentId);
    static Json::Value getEligibleStudents(const std::string& companyId);
    static double calculateRecommendationScore(const Json::Value& student, const Json::Value& company);
};
