#include "TpoService.h"
#include "MongoService.h"
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

Json::Value TpoService::getPendingStudents() {
    auto client = MongoService::instance().acquireClient();
    auto users = MongoService::instance().getCollection(client, "users");
    auto students = MongoService::instance().getCollection(client, "students");

    auto cursor = users.find(
        make_document(kvp("role", "student"), kvp("status", "pending_approval"))
    );

    Json::Value pendingList(Json::arrayValue);
    for (auto& doc : cursor) {
        Json::Value student;
        std::string id = doc["_id"].get_oid().value.to_string();
        student["id"] = id;
        student["name"] = std::string(doc["name"].get_string().value);
        student["email"] = std::string(doc["email"].get_string().value);
        student["status"] = std::string(doc["status"].get_string().value);

        if (doc.find("created_at") != doc.end()) {
            auto dateMs = doc["created_at"].get_date().value.count();
            student["created_at"] = static_cast<Json::Int64>(dateMs);
        }

        // Get student profile for department and roll_number
        auto profileOpt = students.find_one(make_document(kvp("user_id", id)));
        if (profileOpt) {
            auto profile = profileOpt->view();
            if (profile.find("department") != profile.end()) {
                student["department"] = std::string(profile["department"].get_string().value);
            }
            if (profile.find("roll_number") != profile.end()) {
                student["roll_number"] = std::string(profile["roll_number"].get_string().value);
            }
        }

        pendingList.append(student);
    }

    Json::Value result;
    result["success"] = true;
    result["students"] = pendingList;
    return result;
}

Json::Value TpoService::approveStudent(const std::string& userId) {
    auto client = MongoService::instance().acquireClient();
    auto users = MongoService::instance().getCollection(client, "users");

    try {
        auto userOpt = users.find_one(
            make_document(kvp("_id", bsoncxx::oid{userId}))
        );

        if (!userOpt) {
            return JsonHelper::errorResponse("User not found");
        }

        auto userDoc = userOpt->view();
        std::string role = std::string(userDoc["role"].get_string().value);
        if (role != "student") {
            return JsonHelper::errorResponse("Can only approve student accounts");
        }

        std::string currentStatus = "pending_approval";
        if (userDoc.find("status") != userDoc.end()) {
            currentStatus = std::string(userDoc["status"].get_string().value);
        }

        if (currentStatus != "pending_approval") {
            return JsonHelper::errorResponse("Student is not in pending state");
        }

        users.update_one(
            make_document(kvp("_id", bsoncxx::oid{userId})),
            make_document(kvp("$set", make_document(kvp("status", "active"))))
        );

        // Notify the student
        std::string name = std::string(userDoc["name"].get_string().value);
        PlacementService::createNotification(userId,
            "Your account has been approved! You can now log in.",
            "approval");

        Json::Value result;
        result["success"] = true;
        result["message"] = "Student " + name + " approved successfully";
        return result;

    } catch (const std::exception& e) {
        return JsonHelper::errorResponse("Invalid user ID");
    }
}

Json::Value TpoService::rejectStudent(const std::string& userId) {
    auto client = MongoService::instance().acquireClient();
    auto users = MongoService::instance().getCollection(client, "users");

    try {
        auto userOpt = users.find_one(
            make_document(kvp("_id", bsoncxx::oid{userId}))
        );

        if (!userOpt) {
            return JsonHelper::errorResponse("User not found");
        }

        auto userDoc = userOpt->view();
        std::string role = std::string(userDoc["role"].get_string().value);
        if (role != "student") {
            return JsonHelper::errorResponse("Can only reject student accounts");
        }

        users.update_one(
            make_document(kvp("_id", bsoncxx::oid{userId})),
            make_document(kvp("$set", make_document(kvp("status", "rejected"))))
        );

        // Notify the student
        PlacementService::createNotification(userId,
            "Your account registration has been rejected. Contact the placement office for details.",
            "rejection");

        Json::Value result;
        result["success"] = true;
        result["message"] = "Student rejected successfully";
        return result;

    } catch (const std::exception& e) {
        return JsonHelper::errorResponse("Invalid user ID");
    }
}

Json::Value TpoService::createRecruiterAccount(const Json::Value& body, const std::string& tpoId) {
    std::string name = body.get("name", "").asString();
    std::string email = body.get("email", "").asString();
    std::string password = body.get("password", "").asString();
    std::string driveId = body.get("drive_id", "").asString();

    if (name.empty() || email.empty() || password.empty()) {
        return JsonHelper::errorResponse("Recruiter name, email and password are required");
    }

    auto client = MongoService::instance().acquireClient();
    auto users = MongoService::instance().getCollection(client, "users");

    // Check if email exists
    auto existing = users.find_one(make_document(kvp("email", email)));
    if (existing) {
        return JsonHelper::errorResponse("Email already registered");
    }

    std::string hashedPassword = BcryptHelper::hashPassword(password);
    auto now = bsoncxx::types::b_date{std::chrono::system_clock::now()};

    bsoncxx::builder::basic::array drivesArr;
    if (!driveId.empty()) {
        drivesArr.append(driveId);
    }

    auto recruiterDoc = make_document(
        kvp("name", name),
        kvp("email", email),
        kvp("password", hashedPassword),
        kvp("role", "recruiter"),
        kvp("status", "active"),
        kvp("assigned_drives", drivesArr),
        kvp("created_at", now)
    );

    auto insertResult = users.insert_one(recruiterDoc.view());
    if (!insertResult) {
        return JsonHelper::errorResponse("Failed to create recruiter account");
    }

    std::string recruiterId = insertResult->inserted_id().get_oid().value.to_string();

    Json::Value result;
    result["success"] = true;
    result["message"] = "Recruiter account created successfully";
    result["recruiter"]["id"] = recruiterId;
    result["recruiter"]["name"] = name;
    result["recruiter"]["email"] = email;
    result["recruiter"]["password"] = password; // Return plain password so TPO can share

    return result;
}

Json::Value TpoService::getAllRecruiters() {
    auto client = MongoService::instance().acquireClient();
    auto users = MongoService::instance().getCollection(client, "users");
    auto companies = MongoService::instance().getCollection(client, "companies");

    auto cursor = users.find(make_document(kvp("role", "recruiter")));

    Json::Value recruitersList(Json::arrayValue);
    for (auto& doc : cursor) {
        Json::Value recruiter;
        std::string id = doc["_id"].get_oid().value.to_string();
        recruiter["id"] = id;
        recruiter["name"] = std::string(doc["name"].get_string().value);
        recruiter["email"] = std::string(doc["email"].get_string().value);

        if (doc.find("created_at") != doc.end()) {
            auto dateMs = doc["created_at"].get_date().value.count();
            recruiter["created_at"] = static_cast<Json::Int64>(dateMs);
        }

        // Get assigned drives info
        Json::Value drives(Json::arrayValue);
        if (doc.find("assigned_drives") != doc.end()) {
            auto arr = doc["assigned_drives"].get_array().value;
            for (auto& driveId : arr) {
                if (driveId.type() == bsoncxx::type::k_string) {
                    std::string did = std::string(driveId.get_string().value);
                    drives.append(did);

                    // Try to get company name for this drive
                    try {
                        auto companyOpt = companies.find_one(
                            make_document(kvp("_id", bsoncxx::oid{did}))
                        );
                        if (companyOpt) {
                            // Append company info
                        }
                    } catch (...) {}
                }
            }
        }
        recruiter["assigned_drives"] = drives;

        recruitersList.append(recruiter);
    }

    Json::Value result;
    result["success"] = true;
    result["recruiters"] = recruitersList;
    return result;
}
