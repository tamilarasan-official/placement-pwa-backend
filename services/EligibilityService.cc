#include "EligibilityService.h"
#include "MongoService.h"
#include "JsonHelper.h"
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/oid.hpp>
#include <algorithm>
#include <vector>
#include <set>

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;

Json::Value EligibilityService::getEligibleDrives(const std::string& studentId) {
    Json::Value result;

    auto client = MongoService::instance().acquireClient();
    auto students = MongoService::instance().getCollection(client, "students");
    auto studentOpt = students.find_one(make_document(kvp("user_id", studentId)));

    if (!studentOpt) {
        return JsonHelper::errorResponse("Student profile not found");
    }

    auto studentDoc = studentOpt->view();
    double gpa = studentDoc["gpa"].get_double().value;
    int backlogs = studentDoc["backlogs"].get_int32().value;

    // Find drives where student meets eligibility criteria
    auto companies = MongoService::instance().getCollection(client, "companies");
    auto cursor = companies.find(
        make_document(
            kvp("min_gpa", make_document(kvp("$lte", gpa))),
            kvp("allowed_backlogs", make_document(kvp("$gte", backlogs)))
        )
    );

    // Get already applied companies
    auto applications = MongoService::instance().getCollection(client, "applications");
    std::set<std::string> appliedCompanies;
    auto appCursor = applications.find(make_document(kvp("student_id", studentId)));
    for (auto& app : appCursor) {
        appliedCompanies.insert(
            std::string(app["company_id"].get_string().value));
    }

    Json::Value drives(Json::arrayValue);
    for (auto& doc : cursor) {
        Json::Value drive = JsonHelper::bsonToJson(doc);
        std::string companyId;
        if (doc["_id"].type() == bsoncxx::type::k_oid) {
            companyId = doc["_id"].get_oid().value.to_string();
        }
        drive["id"] = companyId;
        drive["already_applied"] = appliedCompanies.count(companyId) > 0;
        drives.append(drive);
    }

    result["success"] = true;
    result["drives"] = drives;
    return result;
}

double EligibilityService::calculateRecommendationScore(const Json::Value& student, const Json::Value& company) {
    double score = 0.0;

    // GPA match (higher GPA relative to min gets higher score)
    double studentGpa = student.get("gpa", 0.0).asDouble();
    double minGpa = company.get("min_gpa", 0.0).asDouble();
    if (studentGpa >= minGpa) {
        score += 30.0 + std::min(20.0, (studentGpa - minGpa) * 10.0);
    }

    // Skills match
    std::set<std::string> studentSkills;
    if (student.isMember("skills") && student["skills"].isArray()) {
        for (const auto& s : student["skills"]) {
            std::string skill = s.asString();
            std::transform(skill.begin(), skill.end(), skill.begin(), ::tolower);
            studentSkills.insert(skill);
        }
    }

    if (company.isMember("required_skills") && company["required_skills"].isArray()) {
        int totalRequired = company["required_skills"].size();
        int matched = 0;
        for (const auto& s : company["required_skills"]) {
            std::string skill = s.asString();
            std::transform(skill.begin(), skill.end(), skill.begin(), ::tolower);
            if (studentSkills.count(skill)) matched++;
        }
        if (totalRequired > 0) {
            score += (static_cast<double>(matched) / totalRequired) * 50.0;
        } else {
            score += 25.0; // No skills required = neutral
        }
    }

    return score;
}

Json::Value EligibilityService::getRecommendedDrives(const std::string& studentId) {
    Json::Value result;

    auto client = MongoService::instance().acquireClient();
    auto students = MongoService::instance().getCollection(client, "students");
    auto studentOpt = students.find_one(make_document(kvp("user_id", studentId)));

    if (!studentOpt) {
        return JsonHelper::errorResponse("Student profile not found");
    }

    Json::Value studentJson = JsonHelper::bsonToJson(studentOpt->view());
    double gpa = studentOpt->view()["gpa"].get_double().value;
    int backlogs = studentOpt->view()["backlogs"].get_int32().value;

    auto companies = MongoService::instance().getCollection(client, "companies");
    auto cursor = companies.find(
        make_document(
            kvp("min_gpa", make_document(kvp("$lte", gpa))),
            kvp("allowed_backlogs", make_document(kvp("$gte", backlogs)))
        )
    );

    std::vector<std::pair<double, Json::Value>> scoredDrives;
    for (auto& doc : cursor) {
        Json::Value drive = JsonHelper::bsonToJson(doc);
        if (doc["_id"].type() == bsoncxx::type::k_oid) {
            drive["id"] = doc["_id"].get_oid().value.to_string();
        }
        double score = calculateRecommendationScore(studentJson, drive);
        drive["recommendation_score"] = score;
        scoredDrives.push_back({score, drive});
    }

    // Sort by score descending
    std::sort(scoredDrives.begin(), scoredDrives.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });

    Json::Value drives(Json::arrayValue);
    for (const auto& [score, drive] : scoredDrives) {
        drives.append(drive);
    }

    result["success"] = true;
    result["drives"] = drives;
    return result;
}

Json::Value EligibilityService::getEligibleStudents(const std::string& companyId) {
    Json::Value result;

    auto client = MongoService::instance().acquireClient();
    auto companies = MongoService::instance().getCollection(client, "companies");
    auto companyOpt = companies.find_one(
        make_document(kvp("_id", bsoncxx::oid{companyId}))
    );

    if (!companyOpt) {
        return JsonHelper::errorResponse("Company drive not found");
    }

    auto companyDoc = companyOpt->view();
    double minGpa = companyDoc["min_gpa"].get_double().value;
    int allowedBacklogs = companyDoc["allowed_backlogs"].get_int32().value;

    auto students = MongoService::instance().getCollection(client, "students");
    auto users = MongoService::instance().getCollection(client, "users");

    auto cursor = students.find(
        make_document(
            kvp("gpa", make_document(kvp("$gte", minGpa))),
            kvp("backlogs", make_document(kvp("$lte", allowedBacklogs)))
        )
    );

    Json::Value studentsList(Json::arrayValue);
    for (auto& doc : cursor) {
        // Only include students whose user account is active
        if (doc.find("user_id") != doc.end()) {
            std::string userId = std::string(doc["user_id"].get_string().value);
            try {
                auto userOpt = users.find_one(
                    make_document(kvp("_id", bsoncxx::oid{userId}))
                );
                if (!userOpt) continue;
                auto userDoc = userOpt->view();
                std::string status = "active";
                if (userDoc.find("status") != userDoc.end()) {
                    status = std::string(userDoc["status"].get_string().value);
                }
                if (status != "active") continue;
            } catch (...) {
                continue;
            }
        }

        Json::Value student = JsonHelper::bsonToJson(doc);
        if (doc.find("user_id") != doc.end()) {
            student["id"] = std::string(doc["user_id"].get_string().value);
        }
        studentsList.append(student);
    }

    result["success"] = true;
    result["students"] = studentsList;
    return result;
}
