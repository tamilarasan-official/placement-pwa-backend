#include "PlacementService.h"
#include "MongoService.h"
#include "JsonHelper.h"
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/builder/basic/array.hpp>
#include <mongocxx/pipeline.hpp>
#include <chrono>
#include <set>
#include <map>

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;
using bsoncxx::builder::basic::make_array;

bool PlacementService::isValidStatusTransition(const std::string& current, const std::string& next) {
    static const std::map<std::string, std::set<std::string>> transitions = {
        {"APPLIED", {"SHORTLISTED", "REJECTED"}},
        {"SHORTLISTED", {"INTERVIEWED", "REJECTED"}},
        {"INTERVIEWED", {"SELECTED", "REJECTED"}},
        {"SELECTED", {}},
        {"REJECTED", {}}
    };

    auto it = transitions.find(current);
    if (it == transitions.end()) return false;
    return it->second.count(next) > 0;
}

Json::Value PlacementService::getAnalytics() {
    Json::Value result;

    auto client = MongoService::instance().acquireClient();
    auto students = MongoService::instance().getCollection(client, "students");
    auto companies = MongoService::instance().getCollection(client, "companies");
    auto applications = MongoService::instance().getCollection(client, "applications");

    // Total counts
    int64_t totalStudents = students.count_documents({});
    int64_t totalCompanies = companies.count_documents({});
    int64_t totalApplications = applications.count_documents({});

    // Placed students count
    int64_t placedStudents = applications.count_documents(
        make_document(kvp("status", "SELECTED"))
    );

    double placementPercentage = totalStudents > 0
        ? (static_cast<double>(placedStudents) / totalStudents) * 100.0
        : 0.0;

    result["success"] = true;
    result["analytics"]["total_students"] = static_cast<Json::Int64>(totalStudents);
    result["analytics"]["total_companies"] = static_cast<Json::Int64>(totalCompanies);
    result["analytics"]["total_applications"] = static_cast<Json::Int64>(totalApplications);
    result["analytics"]["placed_students"] = static_cast<Json::Int64>(placedStudents);
    result["analytics"]["placement_percentage"] = placementPercentage;

    // Application status distribution
    Json::Value statusDist;
    for (const auto& status : {"APPLIED", "SHORTLISTED", "INTERVIEWED", "SELECTED", "REJECTED"}) {
        int64_t count = applications.count_documents(
            make_document(kvp("status", status))
        );
        statusDist[status] = static_cast<Json::Int64>(count);
    }
    result["analytics"]["status_distribution"] = statusDist;

    // Department-wise stats
    mongocxx::pipeline pipe;
    pipe.group(
        make_document(kvp("_id", "$department"),
                      kvp("count", make_document(kvp("$sum", 1))))
    );
    pipe.sort(make_document(kvp("count", -1)));

    Json::Value deptStats(Json::arrayValue);
    auto cursor = students.aggregate(pipe);
    for (auto& doc : cursor) {
        Json::Value dept;
        if (doc["_id"].type() == bsoncxx::type::k_string) {
            dept["department"] = std::string(doc["_id"].get_string().value);
        } else {
            dept["department"] = "Unknown";
        }
        dept["count"] = doc["count"].get_int32().value;
        deptStats.append(dept);
    }
    result["analytics"]["department_stats"] = deptStats;

    return result;
}

Json::Value PlacementService::createNotification(const std::string& userId, const std::string& message, const std::string& type) {
    auto client = MongoService::instance().acquireClient();
    auto notifications = MongoService::instance().getCollection(client, "notifications");
    auto now = bsoncxx::types::b_date{std::chrono::system_clock::now()};

    auto doc = make_document(
        kvp("user_id", userId),
        kvp("message", message),
        kvp("type", type),
        kvp("read", false),
        kvp("created_at", now)
    );

    notifications.insert_one(doc.view());

    Json::Value result;
    result["success"] = true;
    result["message"] = "Notification created";
    return result;
}
