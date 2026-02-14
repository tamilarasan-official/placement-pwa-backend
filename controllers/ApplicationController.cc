#include "ApplicationController.h"
#include "MongoService.h"
#include "PlacementService.h"
#include "JsonHelper.h"
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/oid.hpp>
#include <chrono>

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;

void ApplicationController::updateStatus(const drogon::HttpRequestPtr &req,
                                          std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                                          const std::string &id) {
    auto role = req->attributes()->get<std::string>("role");
    auto userId = req->attributes()->get<std::string>("user_id");

    // Only TPO and recruiter can update status
    if (role != "tpo" && role != "recruiter") {
        auto resp = drogon::HttpResponse::newHttpJsonResponse(
            JsonHelper::errorResponse("Only TPO or recruiter can update application status"));
        resp->setStatusCode(drogon::k403Forbidden);
        callback(resp);
        return;
    }

    auto json = req->getJsonObject();
    if (!json || !json->isMember("status")) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse(
            JsonHelper::errorResponse("status is required"));
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    std::string newStatus = (*json)["status"].asString();

    auto client = MongoService::instance().acquireClient();
    auto applications = MongoService::instance().getCollection(client, "applications");

    try {
        auto appOpt = applications.find_one(
            make_document(kvp("_id", bsoncxx::oid{id}))
        );

        if (!appOpt) {
            auto resp = drogon::HttpResponse::newHttpJsonResponse(
                JsonHelper::errorResponse("Application not found"));
            resp->setStatusCode(drogon::k404NotFound);
            callback(resp);
            return;
        }

        std::string currentStatus = std::string(appOpt->view()["status"].get_string().value);
        std::string companyId = std::string(appOpt->view()["company_id"].get_string().value);
        std::string studentId = std::string(appOpt->view()["student_id"].get_string().value);

        // Recruiter scoping: verify access to this application's drive
        if (role == "recruiter") {
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
                    JsonHelper::errorResponse("Access denied to this application"));
                resp->setStatusCode(drogon::k403Forbidden);
                callback(resp);
                return;
            }
        }

        if (!PlacementService::isValidStatusTransition(currentStatus, newStatus)) {
            auto resp = drogon::HttpResponse::newHttpJsonResponse(
                JsonHelper::errorResponse("Invalid status transition from " + currentStatus + " to " + newStatus));
            resp->setStatusCode(drogon::k400BadRequest);
            callback(resp);
            return;
        }

        applications.update_one(
            make_document(kvp("_id", bsoncxx::oid{id})),
            make_document(kvp("$set", make_document(
                kvp("status", newStatus),
                kvp("updated_at", bsoncxx::types::b_date{std::chrono::system_clock::now()})
            )))
        );

        // Create notification for the student
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

        std::string notifMsg = "Your application status for " + companyName + " has been updated to " + newStatus;
        PlacementService::createNotification(studentId, notifMsg, "status_update");

        // If selected, update student's placement_status
        if (newStatus == "SELECTED") {
            auto students = MongoService::instance().getCollection(client, "students");
            students.update_one(
                make_document(kvp("user_id", studentId)),
                make_document(kvp("$set", make_document(kvp("placement_status", "SELECTED"))))
            );
        }

        Json::Value result;
        result["success"] = true;
        result["message"] = "Application status updated to " + newStatus;
        callback(drogon::HttpResponse::newHttpJsonResponse(result));

    } catch (const std::exception& e) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse(
            JsonHelper::errorResponse("Invalid application ID"));
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
    }
}
