#include "AnalyticsController.h"
#include "MongoService.h"
#include "PlacementService.h"
#include "JsonHelper.h"
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/oid.hpp>

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;

void AnalyticsController::getAnalytics(const drogon::HttpRequestPtr &req,
                                        std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
    auto result = PlacementService::getAnalytics();
    callback(drogon::HttpResponse::newHttpJsonResponse(result));
}

void AnalyticsController::getNotifications(const drogon::HttpRequestPtr &req,
                                            std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
    auto userId = req->attributes()->get<std::string>("user_id");
    auto client = MongoService::instance().acquireClient();
    auto notifications = MongoService::instance().getCollection(client, "notifications");

    mongocxx::options::find opts;
    opts.sort(make_document(kvp("created_at", -1)));
    opts.limit(50);

    auto cursor = notifications.find(
        make_document(kvp("user_id", userId)),
        opts
    );

    Json::Value notifList(Json::arrayValue);
    for (auto& doc : cursor) {
        Json::Value notif = JsonHelper::bsonToJson(doc);
        if (doc["_id"].type() == bsoncxx::type::k_oid) {
            notif["id"] = doc["_id"].get_oid().value.to_string();
        }
        notifList.append(notif);
    }

    // Count unread
    int64_t unreadCount = notifications.count_documents(
        make_document(kvp("user_id", userId), kvp("read", false))
    );

    Json::Value result;
    result["success"] = true;
    result["notifications"] = notifList;
    result["unread_count"] = static_cast<Json::Int64>(unreadCount);
    callback(drogon::HttpResponse::newHttpJsonResponse(result));
}

void AnalyticsController::markNotificationRead(const drogon::HttpRequestPtr &req,
                                                std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                                                const std::string &id) {
    auto client = MongoService::instance().acquireClient();
    auto notifications = MongoService::instance().getCollection(client, "notifications");
    try {
        notifications.update_one(
            make_document(kvp("_id", bsoncxx::oid{id})),
            make_document(kvp("$set", make_document(kvp("read", true))))
        );

        Json::Value result;
        result["success"] = true;
        result["message"] = "Notification marked as read";
        callback(drogon::HttpResponse::newHttpJsonResponse(result));
    } catch (const std::exception& e) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse(
            JsonHelper::errorResponse("Invalid notification ID"));
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
    }
}

void AnalyticsController::getAllStudents(const drogon::HttpRequestPtr &req,
                                          std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
    auto client = MongoService::instance().acquireClient();
    auto students = MongoService::instance().getCollection(client, "students");
    auto cursor = students.find({});

    Json::Value studentsList(Json::arrayValue);
    for (auto& doc : cursor) {
        Json::Value student = JsonHelper::bsonToJson(doc);
        if (doc.find("user_id") != doc.end()) {
            student["id"] = std::string(doc["user_id"].get_string().value);
        }
        studentsList.append(student);
    }

    Json::Value result;
    result["success"] = true;
    result["students"] = studentsList;
    callback(drogon::HttpResponse::newHttpJsonResponse(result));
}

void AnalyticsController::getAllApplications(const drogon::HttpRequestPtr &req,
                                              std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
    auto client = MongoService::instance().acquireClient();
    auto applications = MongoService::instance().getCollection(client, "applications");
    auto students = MongoService::instance().getCollection(client, "students");
    auto companies = MongoService::instance().getCollection(client, "companies");

    auto cursor = applications.find({});

    Json::Value appsList(Json::arrayValue);
    for (auto& doc : cursor) {
        Json::Value app = JsonHelper::bsonToJson(doc);
        if (doc["_id"].type() == bsoncxx::type::k_oid) {
            app["id"] = doc["_id"].get_oid().value.to_string();
        }

        // Attach student info
        std::string studentId = std::string(doc["student_id"].get_string().value);
        auto studentOpt = students.find_one(make_document(kvp("user_id", studentId)));
        if (studentOpt) {
            app["student"] = JsonHelper::bsonToJson(studentOpt->view());
        }

        // Attach company info
        std::string companyId = std::string(doc["company_id"].get_string().value);
        try {
            auto companyOpt = companies.find_one(
                make_document(kvp("_id", bsoncxx::oid{companyId}))
            );
            if (companyOpt) {
                app["company"] = JsonHelper::bsonToJson(companyOpt->view());
                app["company"]["id"] = companyId;
            }
        } catch (...) {}

        appsList.append(app);
    }

    Json::Value result;
    result["success"] = true;
    result["applications"] = appsList;
    callback(drogon::HttpResponse::newHttpJsonResponse(result));
}
