#include "CompanyController.h"
#include "MongoService.h"
#include "EligibilityService.h"
#include "BcryptHelper.h"
#include "PlacementService.h"
#include "JsonHelper.h"
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/oid.hpp>
#include <chrono>

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;
using bsoncxx::builder::basic::make_array;

void CompanyController::createCompany(const drogon::HttpRequestPtr &req,
                                       std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
    auto json = req->getJsonObject();
    if (!json) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse(
            JsonHelper::errorResponse("Invalid JSON body"));
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    auto tpoId = req->attributes()->get<std::string>("user_id");

    std::string companyName = json->get("company_name", "").asString();
    std::string role = json->get("role", "").asString();
    double minGpa = json->get("min_gpa", 0.0).asDouble();
    int allowedBacklogs = json->get("allowed_backlogs", 0).asInt();
    std::string driveDate = json->get("drive_date", "").asString();

    if (companyName.empty() || role.empty()) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse(
            JsonHelper::errorResponse("company_name and role are required"));
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    bsoncxx::builder::basic::array skillsArr;
    if (json->isMember("required_skills") && (*json)["required_skills"].isArray()) {
        for (const auto& skill : (*json)["required_skills"]) {
            skillsArr.append(skill.asString());
        }
    }

    auto client = MongoService::instance().acquireClient();
    auto users = MongoService::instance().getCollection(client, "users");
    auto companies = MongoService::instance().getCollection(client, "companies");

    // Handle recruiter creation/assignment
    std::string recruiterId = "";
    std::string recruiterName = json->get("recruiter_name", "").asString();
    std::string recruiterEmail = json->get("recruiter_email", "").asString();
    std::string recruiterPassword = json->get("recruiter_password", "").asString();
    std::string existingRecruiterId = json->get("existing_recruiter_id", "").asString();

    // First create the company drive
    auto now = bsoncxx::types::b_date{std::chrono::system_clock::now()};

    bsoncxx::builder::basic::document docBuilder;
    docBuilder.append(kvp("company_name", companyName));
    docBuilder.append(kvp("role", role));
    docBuilder.append(kvp("min_gpa", minGpa));
    docBuilder.append(kvp("allowed_backlogs", allowedBacklogs));
    docBuilder.append(kvp("required_skills", skillsArr));
    docBuilder.append(kvp("drive_date", driveDate));
    docBuilder.append(kvp("created_by", tpoId));
    docBuilder.append(kvp("created_at", now));

    // Will set recruiter_id after potential creation
    auto companyResult = companies.insert_one(docBuilder.extract());
    if (!companyResult) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse(
            JsonHelper::errorResponse("Failed to create company drive"));
        resp->setStatusCode(drogon::k500InternalServerError);
        callback(resp);
        return;
    }

    std::string companyId = companyResult->inserted_id().get_oid().value.to_string();

    Json::Value res;
    res["success"] = true;
    res["message"] = "Company drive created successfully";
    res["id"] = companyId;

    if (!existingRecruiterId.empty()) {
        // Use existing recruiter - push drive to their assigned_drives
        try {
            users.update_one(
                make_document(kvp("_id", bsoncxx::oid{existingRecruiterId})),
                make_document(kvp("$push", make_document(kvp("assigned_drives", companyId))))
            );
            recruiterId = existingRecruiterId;
        } catch (...) {}
    } else if (!recruiterName.empty() && !recruiterEmail.empty() && !recruiterPassword.empty()) {
        // Create new recruiter account
        auto existingUser = users.find_one(make_document(kvp("email", recruiterEmail)));
        if (existingUser) {
            // Email already exists - still create drive but report recruiter issue
            res["recruiter_error"] = "Recruiter email already registered";
        } else {
            std::string hashedPassword = BcryptHelper::hashPassword(recruiterPassword);

            bsoncxx::builder::basic::array drivesArr;
            drivesArr.append(companyId);

            auto recruiterDoc = make_document(
                kvp("name", recruiterName),
                kvp("email", recruiterEmail),
                kvp("password", hashedPassword),
                kvp("role", "recruiter"),
                kvp("status", "active"),
                kvp("assigned_drives", drivesArr),
                kvp("created_at", now)
            );

            auto recruiterResult = users.insert_one(recruiterDoc.view());
            if (recruiterResult) {
                recruiterId = recruiterResult->inserted_id().get_oid().value.to_string();
                res["recruiter"]["id"] = recruiterId;
                res["recruiter"]["name"] = recruiterName;
                res["recruiter"]["email"] = recruiterEmail;
                res["recruiter"]["password"] = recruiterPassword;
            }
        }
    }

    // Update company with recruiter_id if we have one
    if (!recruiterId.empty()) {
        companies.update_one(
            make_document(kvp("_id", bsoncxx::oid{companyId})),
            make_document(kvp("$set", make_document(kvp("recruiter_id", recruiterId))))
        );
    }

    callback(drogon::HttpResponse::newHttpJsonResponse(res));
}

void CompanyController::getAllCompanies(const drogon::HttpRequestPtr &req,
                                        std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
    auto role = req->attributes()->get<std::string>("role");
    auto userId = req->attributes()->get<std::string>("user_id");

    auto client = MongoService::instance().acquireClient();
    auto companies = MongoService::instance().getCollection(client, "companies");

    mongocxx::cursor cursor = [&]() {
        if (role == "recruiter") {
            // Recruiters can only see their assigned drives
            auto users = MongoService::instance().getCollection(client, "users");
            auto userOpt = users.find_one(
                make_document(kvp("_id", bsoncxx::oid{userId}))
            );

            if (userOpt && userOpt->view().find("assigned_drives") != userOpt->view().end()) {
                bsoncxx::builder::basic::array driveOids;
                auto arr = userOpt->view()["assigned_drives"].get_array().value;
                for (auto& driveId : arr) {
                    if (driveId.type() == bsoncxx::type::k_string) {
                        try {
                            driveOids.append(bsoncxx::oid{std::string(driveId.get_string().value)});
                        } catch (...) {}
                    }
                }
                return companies.find(
                    make_document(kvp("_id", make_document(kvp("$in", driveOids))))
                );
            }
            // No assigned drives - return empty
            return companies.find(
                make_document(kvp("_id", make_document(kvp("$in", make_array()))))
            );
        }
        // TPO and students see all drives
        return companies.find({});
    }();

    Json::Value companiesList(Json::arrayValue);
    for (auto& doc : cursor) {
        Json::Value company = JsonHelper::bsonToJson(doc);
        if (doc["_id"].type() == bsoncxx::type::k_oid) {
            company["id"] = doc["_id"].get_oid().value.to_string();
        }
        companiesList.append(company);
    }

    Json::Value result;
    result["success"] = true;
    result["companies"] = companiesList;
    callback(drogon::HttpResponse::newHttpJsonResponse(result));
}

void CompanyController::getCompany(const drogon::HttpRequestPtr &req,
                                    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                                    const std::string &id) {
    auto role = req->attributes()->get<std::string>("role");
    auto userId = req->attributes()->get<std::string>("user_id");

    auto client = MongoService::instance().acquireClient();
    auto companies = MongoService::instance().getCollection(client, "companies");
    try {
        auto companyOpt = companies.find_one(
            make_document(kvp("_id", bsoncxx::oid{id}))
        );

        if (!companyOpt) {
            auto resp = drogon::HttpResponse::newHttpJsonResponse(
                JsonHelper::errorResponse("Company not found"));
            resp->setStatusCode(drogon::k404NotFound);
            callback(resp);
            return;
        }

        // Recruiter scoping check
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
                        std::string(driveId.get_string().value) == id) {
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

        Json::Value result;
        result["success"] = true;
        result["company"] = JsonHelper::bsonToJson(companyOpt->view());
        result["company"]["id"] = id;
        callback(drogon::HttpResponse::newHttpJsonResponse(result));
    } catch (const std::exception& e) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse(
            JsonHelper::errorResponse("Invalid company ID"));
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
    }
}

void CompanyController::updateCompany(const drogon::HttpRequestPtr &req,
                                       std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                                       const std::string &id) {
    auto json = req->getJsonObject();
    if (!json) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse(
            JsonHelper::errorResponse("Invalid JSON body"));
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    bsoncxx::builder::basic::document setDoc;
    if (json->isMember("company_name")) setDoc.append(kvp("company_name", (*json)["company_name"].asString()));
    if (json->isMember("role")) setDoc.append(kvp("role", (*json)["role"].asString()));
    if (json->isMember("min_gpa")) setDoc.append(kvp("min_gpa", (*json)["min_gpa"].asDouble()));
    if (json->isMember("allowed_backlogs")) setDoc.append(kvp("allowed_backlogs", (*json)["allowed_backlogs"].asInt()));
    if (json->isMember("drive_date")) setDoc.append(kvp("drive_date", (*json)["drive_date"].asString()));

    if (json->isMember("required_skills") && (*json)["required_skills"].isArray()) {
        bsoncxx::builder::basic::array skillsArr;
        for (const auto& skill : (*json)["required_skills"]) {
            skillsArr.append(skill.asString());
        }
        setDoc.append(kvp("required_skills", skillsArr));
    }

    auto client = MongoService::instance().acquireClient();
    auto companies = MongoService::instance().getCollection(client, "companies");
    try {
        auto result = companies.update_one(
            make_document(kvp("_id", bsoncxx::oid{id})),
            make_document(kvp("$set", setDoc))
        );

        if (result && result->matched_count() > 0) {
            Json::Value res;
            res["success"] = true;
            res["message"] = "Company drive updated successfully";
            callback(drogon::HttpResponse::newHttpJsonResponse(res));
        } else {
            auto resp = drogon::HttpResponse::newHttpJsonResponse(
                JsonHelper::errorResponse("Company not found"));
            resp->setStatusCode(drogon::k404NotFound);
            callback(resp);
        }
    } catch (const std::exception& e) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse(
            JsonHelper::errorResponse("Invalid company ID"));
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
    }
}

void CompanyController::deleteCompany(const drogon::HttpRequestPtr &req,
                                       std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                                       const std::string &id) {
    auto client = MongoService::instance().acquireClient();
    auto companies = MongoService::instance().getCollection(client, "companies");
    try {
        auto result = companies.delete_one(
            make_document(kvp("_id", bsoncxx::oid{id}))
        );

        if (result && result->deleted_count() > 0) {
            Json::Value res;
            res["success"] = true;
            res["message"] = "Company drive deleted successfully";
            callback(drogon::HttpResponse::newHttpJsonResponse(res));
        } else {
            auto resp = drogon::HttpResponse::newHttpJsonResponse(
                JsonHelper::errorResponse("Company not found"));
            resp->setStatusCode(drogon::k404NotFound);
            callback(resp);
        }
    } catch (const std::exception& e) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse(
            JsonHelper::errorResponse("Invalid company ID"));
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
    }
}

void CompanyController::getEligibleStudents(const drogon::HttpRequestPtr &req,
                                             std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                                             const std::string &id) {
    auto role = req->attributes()->get<std::string>("role");
    auto userId = req->attributes()->get<std::string>("user_id");

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
                    std::string(driveId.get_string().value) == id) {
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

    auto result = EligibilityService::getEligibleStudents(id);
    auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
    if (!result["success"].asBool()) {
        resp->setStatusCode(drogon::k404NotFound);
    }
    callback(resp);
}

void CompanyController::getDriveApplications(const drogon::HttpRequestPtr &req,
                                              std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                                              const std::string &id) {
    auto role = req->attributes()->get<std::string>("role");
    auto userId = req->attributes()->get<std::string>("user_id");

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
                    std::string(driveId.get_string().value) == id) {
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
    auto applications = MongoService::instance().getCollection(client, "applications");
    auto students = MongoService::instance().getCollection(client, "students");

    auto cursor = applications.find(make_document(kvp("company_id", id)));

    Json::Value apps(Json::arrayValue);
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

        apps.append(app);
    }

    Json::Value result;
    result["success"] = true;
    result["applications"] = apps;
    callback(drogon::HttpResponse::newHttpJsonResponse(result));
}
