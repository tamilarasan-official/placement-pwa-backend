#include "InterviewController.h"
#include "MongoService.h"
#include "PlacementService.h"
#include "JsonHelper.h"
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/oid.hpp>
#include <chrono>
#include <set>

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;

void InterviewController::scheduleInterview(const drogon::HttpRequestPtr &req,
                                             std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
    auto role = req->attributes()->get<std::string>("role");
    auto userId = req->attributes()->get<std::string>("user_id");

    // Only TPO and recruiter can schedule interviews
    if (role != "tpo" && role != "recruiter") {
        auto resp = drogon::HttpResponse::newHttpJsonResponse(
            JsonHelper::errorResponse("Only TPO or recruiter can schedule interviews"));
        resp->setStatusCode(drogon::k403Forbidden);
        callback(resp);
        return;
    }

    auto json = req->getJsonObject();
    if (!json) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse(
            JsonHelper::errorResponse("Invalid JSON body"));
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    std::string studentId = json->get("student_id", "").asString();
    std::string companyId = json->get("company_id", "").asString();
    std::string interviewDate = json->get("interview_date", "").asString();
    std::string interviewTime = json->get("interview_time", "").asString();
    std::string mode = json->get("mode", "online").asString();

    if (studentId.empty() || companyId.empty() || interviewDate.empty()) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse(
            JsonHelper::errorResponse("student_id, company_id, and interview_date are required"));
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    // Recruiter scoping check
    if (role == "recruiter") {
        auto client = MongoService::instance().acquireClient();
        auto users = MongoService::instance().getCollection(client, "users");
        auto userOpt = users.find_one(
            make_document(kvp("_id", bsoncxx::oid{userId}))
        );
        bool hasAccess = false;
        if (userOpt && userOpt->view().find("assigned_drives") != userOpt->view().end()) {
            auto arr = userOpt->view()["assigned_drives"].get_array().value;
            for (auto& driveId : arr) {
                if (driveId.type() == bsoncxx::type::k_string &&
                    std::string(driveId.get_string().value) == companyId) {
                    hasAccess = true;
                    break;
                }
            }
        }
        if (!hasAccess) {
            auto resp = drogon::HttpResponse::newHttpJsonResponse(
                JsonHelper::errorResponse("Access denied to this drive"));
            resp->setStatusCode(drogon::k403Forbidden);
            callback(resp);
            return;
        }
    }

    auto client = MongoService::instance().acquireClient();
    auto interviews = MongoService::instance().getCollection(client, "interviews");
    auto now = bsoncxx::types::b_date{std::chrono::system_clock::now()};

    auto doc = make_document(
        kvp("student_id", studentId),
        kvp("company_id", companyId),
        kvp("interview_date", interviewDate),
        kvp("interview_time", interviewTime),
        kvp("mode", mode),
        kvp("created_at", now)
    );

    auto result = interviews.insert_one(doc.view());

    // Notify student
    auto companies = MongoService::instance().getCollection(client, "companies");
    std::string companyName = "a company";
    try {
        auto companyOpt = companies.find_one(
            make_document(kvp("_id", bsoncxx::oid{companyId}))
        );
        if (companyOpt) {
            companyName = std::string(companyOpt->view()["company_name"].get_string().value);
        }
    } catch (...) {}

    PlacementService::createNotification(studentId,
        "Interview scheduled with " + companyName + " on " + interviewDate + " (" + mode + ")",
        "interview");

    Json::Value res;
    res["success"] = true;
    res["message"] = "Interview scheduled successfully";
    if (result) {
        res["id"] = result->inserted_id().get_oid().value.to_string();
    }
    callback(drogon::HttpResponse::newHttpJsonResponse(res));
}

void InterviewController::getAllInterviews(const drogon::HttpRequestPtr &req,
                                            std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
    auto role = req->attributes()->get<std::string>("role");
    auto userId = req->attributes()->get<std::string>("user_id");

    // Only TPO and recruiter can view all interviews
    if (role != "tpo" && role != "recruiter") {
        auto resp = drogon::HttpResponse::newHttpJsonResponse(
            JsonHelper::errorResponse("Access denied"));
        resp->setStatusCode(drogon::k403Forbidden);
        callback(resp);
        return;
    }

    auto client = MongoService::instance().acquireClient();
    auto interviews = MongoService::instance().getCollection(client, "interviews");
    auto companies = MongoService::instance().getCollection(client, "companies");
    auto students = MongoService::instance().getCollection(client, "students");

    // For recruiters, get their assigned drives and only show those interviews
    std::set<std::string> allowedDrives;
    if (role == "recruiter") {
        auto users = MongoService::instance().getCollection(client, "users");
        auto userOpt = users.find_one(
            make_document(kvp("_id", bsoncxx::oid{userId}))
        );
        if (userOpt && userOpt->view().find("assigned_drives") != userOpt->view().end()) {
            auto arr = userOpt->view()["assigned_drives"].get_array().value;
            for (auto& driveId : arr) {
                if (driveId.type() == bsoncxx::type::k_string) {
                    allowedDrives.insert(std::string(driveId.get_string().value));
                }
            }
        }
    }

    // Optionally filter by company_id query param
    std::string companyIdFilter = req->getParameter("company_id");

    mongocxx::cursor cursor = companyIdFilter.empty()
        ? interviews.find({})
        : interviews.find(make_document(kvp("company_id", companyIdFilter)));

    Json::Value interviewList(Json::arrayValue);
    for (auto& doc : cursor) {
        std::string companyId = std::string(doc["company_id"].get_string().value);

        // Recruiter scoping - skip interviews for drives they don't have access to
        if (role == "recruiter" && allowedDrives.find(companyId) == allowedDrives.end()) {
            continue;
        }

        Json::Value interview = JsonHelper::bsonToJson(doc);
        if (doc["_id"].type() == bsoncxx::type::k_oid) {
            interview["id"] = doc["_id"].get_oid().value.to_string();
        }

        // Attach company info
        try {
            auto companyOpt = companies.find_one(
                make_document(kvp("_id", bsoncxx::oid{companyId}))
            );
            if (companyOpt) {
                interview["company"] = JsonHelper::bsonToJson(companyOpt->view());
                interview["company"]["id"] = companyId;
            }
        } catch (...) {}

        // Attach student info
        std::string studentId = std::string(doc["student_id"].get_string().value);
        auto studentOpt = students.find_one(make_document(kvp("user_id", studentId)));
        if (studentOpt) {
            interview["student"] = JsonHelper::bsonToJson(studentOpt->view());
        }

        interviewList.append(interview);
    }

    Json::Value result;
    result["success"] = true;
    result["interviews"] = interviewList;
    callback(drogon::HttpResponse::newHttpJsonResponse(result));
}

void InterviewController::getMyInterviews(const drogon::HttpRequestPtr &req,
                                           std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
    auto userId = req->attributes()->get<std::string>("user_id");
    auto client = MongoService::instance().acquireClient();
    auto interviews = MongoService::instance().getCollection(client, "interviews");
    auto companies = MongoService::instance().getCollection(client, "companies");

    auto cursor = interviews.find(make_document(kvp("student_id", userId)));

    Json::Value interviewList(Json::arrayValue);
    for (auto& doc : cursor) {
        Json::Value interview = JsonHelper::bsonToJson(doc);
        if (doc["_id"].type() == bsoncxx::type::k_oid) {
            interview["id"] = doc["_id"].get_oid().value.to_string();
        }

        std::string companyId = std::string(doc["company_id"].get_string().value);
        try {
            auto companyOpt = companies.find_one(
                make_document(kvp("_id", bsoncxx::oid{companyId}))
            );
            if (companyOpt) {
                interview["company"] = JsonHelper::bsonToJson(companyOpt->view());
                interview["company"]["id"] = companyId;
            }
        } catch (...) {}

        interviewList.append(interview);
    }

    Json::Value result;
    result["success"] = true;
    result["interviews"] = interviewList;
    callback(drogon::HttpResponse::newHttpJsonResponse(result));
}
