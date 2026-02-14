#include "StudentController.h"
#include "MongoService.h"
#include "EligibilityService.h"
#include "PlacementService.h"
#include "JsonHelper.h"
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/oid.hpp>
#include <chrono>
#include <fstream>

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;
using bsoncxx::builder::basic::make_array;

void StudentController::getProfile(const drogon::HttpRequestPtr &req,
                                    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
    auto userId = req->attributes()->get<std::string>("user_id");
    auto client = MongoService::instance().acquireClient();
    auto students = MongoService::instance().getCollection(client, "students");

    auto studentOpt = students.find_one(make_document(kvp("user_id", userId)));

    if (!studentOpt) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse(
            JsonHelper::errorResponse("Student profile not found"));
        resp->setStatusCode(drogon::k404NotFound);
        callback(resp);
        return;
    }

    Json::Value result;
    result["success"] = true;
    result["profile"] = JsonHelper::bsonToJson(studentOpt->view());
    result["profile"]["id"] = userId;

    callback(drogon::HttpResponse::newHttpJsonResponse(result));
}

void StudentController::updateProfile(const drogon::HttpRequestPtr &req,
                                       std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
    auto userId = req->attributes()->get<std::string>("user_id");
    auto json = req->getJsonObject();

    if (!json) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse(
            JsonHelper::errorResponse("Invalid JSON body"));
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    auto client = MongoService::instance().acquireClient();
    auto students = MongoService::instance().getCollection(client, "students");

    bsoncxx::builder::basic::document updateDoc;
    bsoncxx::builder::basic::document setDoc;

    if (json->isMember("name")) setDoc.append(kvp("name", (*json)["name"].asString()));
    if (json->isMember("department")) setDoc.append(kvp("department", (*json)["department"].asString()));
    if (json->isMember("gpa")) setDoc.append(kvp("gpa", (*json)["gpa"].asDouble()));
    if (json->isMember("backlogs")) setDoc.append(kvp("backlogs", (*json)["backlogs"].asInt()));
    if (json->isMember("github")) setDoc.append(kvp("github", (*json)["github"].asString()));
    if (json->isMember("linkedin")) setDoc.append(kvp("linkedin", (*json)["linkedin"].asString()));
    if (json->isMember("portfolio")) setDoc.append(kvp("portfolio", (*json)["portfolio"].asString()));

    if (json->isMember("skills") && (*json)["skills"].isArray()) {
        bsoncxx::builder::basic::array skillsArr;
        for (const auto& skill : (*json)["skills"]) {
            skillsArr.append(skill.asString());
        }
        setDoc.append(kvp("skills", skillsArr));
    }

    updateDoc.append(kvp("$set", setDoc));

    auto result = students.update_one(
        make_document(kvp("user_id", userId)),
        updateDoc.extract()
    );

    if (result && result->modified_count() > 0) {
        Json::Value res;
        res["success"] = true;
        res["message"] = "Profile updated successfully";
        callback(drogon::HttpResponse::newHttpJsonResponse(res));
    } else {
        auto resp = drogon::HttpResponse::newHttpJsonResponse(
            JsonHelper::errorResponse("Failed to update profile"));
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
    }
}

void StudentController::uploadResume(const drogon::HttpRequestPtr &req,
                                      std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
    auto userId = req->attributes()->get<std::string>("user_id");

    drogon::MultiPartParser fileParser;
    if (fileParser.parse(req) != 0) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse(
            JsonHelper::errorResponse("Failed to parse file upload"));
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    auto &files = fileParser.getFiles();
    if (files.empty()) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse(
            JsonHelper::errorResponse("No file uploaded"));
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    auto &file = files[0];
    std::string ext(file.getFileExtension());

    if (ext != "pdf") {
        auto resp = drogon::HttpResponse::newHttpJsonResponse(
            JsonHelper::errorResponse("Only PDF files are allowed"));
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    // Max 5MB
    if (file.fileLength() > 5 * 1024 * 1024) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse(
            JsonHelper::errorResponse("File size must be under 5MB"));
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    std::string filename = "resume_" + userId + ".pdf";
    std::string savePath = "./uploads/" + filename;
    file.saveAs(savePath);

    std::string resumeUrl = "/uploads/" + filename;

    auto client = MongoService::instance().acquireClient();
    auto students = MongoService::instance().getCollection(client, "students");
    students.update_one(
        make_document(kvp("user_id", userId)),
        make_document(kvp("$set", make_document(kvp("resume_url", resumeUrl))))
    );

    Json::Value result;
    result["success"] = true;
    result["resume_url"] = resumeUrl;
    result["message"] = "Resume uploaded successfully";
    callback(drogon::HttpResponse::newHttpJsonResponse(result));
}

void StudentController::getEligibleDrives(const drogon::HttpRequestPtr &req,
                                           std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
    auto userId = req->attributes()->get<std::string>("user_id");
    auto result = EligibilityService::getEligibleDrives(userId);
    auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
    if (!result["success"].asBool()) {
        resp->setStatusCode(drogon::k404NotFound);
    }
    callback(resp);
}

void StudentController::getRecommendedDrives(const drogon::HttpRequestPtr &req,
                                              std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
    auto userId = req->attributes()->get<std::string>("user_id");
    auto result = EligibilityService::getRecommendedDrives(userId);
    auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
    if (!result["success"].asBool()) {
        resp->setStatusCode(drogon::k404NotFound);
    }
    callback(resp);
}

void StudentController::applyToDrive(const drogon::HttpRequestPtr &req,
                                      std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
    auto userId = req->attributes()->get<std::string>("user_id");
    auto json = req->getJsonObject();

    if (!json || !json->isMember("company_id")) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse(
            JsonHelper::errorResponse("company_id is required"));
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    std::string companyId = (*json)["company_id"].asString();

    auto client = MongoService::instance().acquireClient();

    // Verify company exists
    auto companies = MongoService::instance().getCollection(client, "companies");
    auto companyOpt = companies.find_one(
        make_document(kvp("_id", bsoncxx::oid{companyId}))
    );
    if (!companyOpt) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse(
            JsonHelper::errorResponse("Company drive not found"));
        resp->setStatusCode(drogon::k404NotFound);
        callback(resp);
        return;
    }

    // Check if already applied
    auto applications = MongoService::instance().getCollection(client, "applications");
    auto existingApp = applications.find_one(
        make_document(kvp("student_id", userId), kvp("company_id", companyId))
    );
    if (existingApp) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse(
            JsonHelper::errorResponse("Already applied to this drive"));
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    // Create application
    auto now = bsoncxx::types::b_date{std::chrono::system_clock::now()};
    auto appDoc = make_document(
        kvp("student_id", userId),
        kvp("company_id", companyId),
        kvp("status", "APPLIED"),
        kvp("applied_at", now)
    );

    applications.insert_one(appDoc.view());

    // Create notification for student
    auto companyDoc = companyOpt->view();
    std::string companyName = std::string(companyDoc["company_name"].get_string().value);
    PlacementService::createNotification(userId,
        "You have applied to " + companyName, "application");

    Json::Value result;
    result["success"] = true;
    result["message"] = "Application submitted successfully";
    callback(drogon::HttpResponse::newHttpJsonResponse(result));
}

void StudentController::getApplications(const drogon::HttpRequestPtr &req,
                                         std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
    auto userId = req->attributes()->get<std::string>("user_id");
    auto client = MongoService::instance().acquireClient();
    auto applications = MongoService::instance().getCollection(client, "applications");
    auto companies = MongoService::instance().getCollection(client, "companies");

    auto cursor = applications.find(make_document(kvp("student_id", userId)));

    Json::Value apps(Json::arrayValue);
    for (auto& doc : cursor) {
        Json::Value app = JsonHelper::bsonToJson(doc);
        if (doc["_id"].type() == bsoncxx::type::k_oid) {
            app["id"] = doc["_id"].get_oid().value.to_string();
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

        apps.append(app);
    }

    Json::Value result;
    result["success"] = true;
    result["applications"] = apps;
    callback(drogon::HttpResponse::newHttpJsonResponse(result));
}
