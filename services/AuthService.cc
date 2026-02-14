#include "AuthService.h"
#include "MongoService.h"
#include "BcryptHelper.h"
#include "JwtHelper.h"
#include "JsonHelper.h"
#include "PlacementService.h"
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/oid.hpp>
#include <chrono>

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;
using bsoncxx::builder::basic::make_array;

Json::Value AuthService::registerUser(const Json::Value& body) {
    Json::Value result;

    std::string name = body.get("name", "").asString();
    std::string email = body.get("email", "").asString();
    std::string password = body.get("password", "").asString();
    std::string department = body.get("department", "").asString();
    std::string rollNumber = body.get("roll_number", "").asString();

    // Only students can register publicly
    std::string role = "student";

    if (name.empty() || email.empty() || password.empty()) {
        return JsonHelper::errorResponse("Name, email and password are required");
    }

    if (department.empty()) {
        return JsonHelper::errorResponse("Department is required");
    }

    if (rollNumber.empty()) {
        return JsonHelper::errorResponse("Roll number is required");
    }

    if (password.length() < 6) {
        return JsonHelper::errorResponse("Password must be at least 6 characters");
    }

    auto client = MongoService::instance().acquireClient();
    auto users = MongoService::instance().getCollection(client, "users");

    // Check if email exists
    auto existing = users.find_one(make_document(kvp("email", email)));
    if (existing) {
        return JsonHelper::errorResponse("Email already registered");
    }

    // Hash password
    std::string hashedPassword = BcryptHelper::hashPassword(password);

    // Create user document with pending_approval status
    auto now = bsoncxx::types::b_date{std::chrono::system_clock::now()};
    auto userDoc = make_document(
        kvp("name", name),
        kvp("email", email),
        kvp("password", hashedPassword),
        kvp("role", role),
        kvp("status", "pending_approval"),
        kvp("assigned_drives", make_array()),
        kvp("created_at", now)
    );

    auto insertResult = users.insert_one(userDoc.view());
    if (!insertResult) {
        return JsonHelper::errorResponse("Failed to create user");
    }

    std::string userId = insertResult->inserted_id().get_oid().value.to_string();

    // Create student profile with department and roll_number
    auto students = MongoService::instance().getCollection(client, "students");
    auto studentDoc = make_document(
        kvp("user_id", userId),
        kvp("name", name),
        kvp("department", department),
        kvp("roll_number", rollNumber),
        kvp("gpa", 0.0),
        kvp("backlogs", 0),
        kvp("skills", make_array()),
        kvp("resume_url", ""),
        kvp("github", ""),
        kvp("linkedin", ""),
        kvp("portfolio", ""),
        kvp("placement_status", "NOT_APPLIED"),
        kvp("created_at", now)
    );
    students.insert_one(studentDoc.view());

    // Create notification for TPO about new pending registration
    // Find TPO user(s)
    auto tpoCursor = users.find(make_document(kvp("role", "tpo")));
    for (auto& tpoDoc : tpoCursor) {
        std::string tpoId = tpoDoc["_id"].get_oid().value.to_string();
        PlacementService::createNotification(tpoId,
            "New student registration awaiting approval: " + name + " (" + email + ")",
            "registration");
    }

    // Return success but NO token (student cannot login until approved)
    result["success"] = true;
    result["message"] = "Registration successful. Your account is pending TPO approval.";

    return result;
}

Json::Value AuthService::loginUser(const Json::Value& body) {
    std::string email = body.get("email", "").asString();
    std::string password = body.get("password", "").asString();

    if (email.empty() || password.empty()) {
        return JsonHelper::errorResponse("Email and password are required");
    }

    auto client = MongoService::instance().acquireClient();
    auto users = MongoService::instance().getCollection(client, "users");
    auto userOpt = users.find_one(make_document(kvp("email", email)));

    if (!userOpt) {
        return JsonHelper::errorResponse("Invalid email or password");
    }

    auto userDoc = userOpt->view();
    std::string storedHash = std::string(userDoc["password"].get_string().value);

    if (!BcryptHelper::verifyPassword(password, storedHash)) {
        return JsonHelper::errorResponse("Invalid email or password");
    }

    // Check user status
    std::string status = "active";
    if (userDoc.find("status") != userDoc.end()) {
        status = std::string(userDoc["status"].get_string().value);
    }

    if (status == "pending_approval") {
        Json::Value result;
        result["success"] = false;
        result["error"] = "Your account is pending TPO approval";
        result["status"] = "pending_approval";
        return result;
    }

    if (status == "rejected") {
        Json::Value result;
        result["success"] = false;
        result["error"] = "Your account has been rejected. Contact the placement office.";
        result["status"] = "rejected";
        return result;
    }

    std::string userId = userDoc["_id"].get_oid().value.to_string();
    std::string name = std::string(userDoc["name"].get_string().value);
    std::string role = std::string(userDoc["role"].get_string().value);

    std::string token = JwtHelper::generateToken(userId, role);

    Json::Value result;
    result["success"] = true;
    result["token"] = token;
    result["user"]["id"] = userId;
    result["user"]["name"] = name;
    result["user"]["email"] = email;
    result["user"]["role"] = role;
    result["user"]["status"] = status;

    return result;
}

Json::Value AuthService::getUserById(const std::string& userId) {
    auto client = MongoService::instance().acquireClient();
    auto users = MongoService::instance().getCollection(client, "users");
    auto userOpt = users.find_one(
        make_document(kvp("_id", bsoncxx::oid{userId}))
    );

    if (!userOpt) {
        return JsonHelper::errorResponse("User not found");
    }

    auto userDoc = userOpt->view();

    std::string status = "active";
    if (userDoc.find("status") != userDoc.end()) {
        status = std::string(userDoc["status"].get_string().value);
    }

    Json::Value result;
    result["success"] = true;
    result["user"]["id"] = userId;
    result["user"]["name"] = std::string(userDoc["name"].get_string().value);
    result["user"]["email"] = std::string(userDoc["email"].get_string().value);
    result["user"]["role"] = std::string(userDoc["role"].get_string().value);
    result["user"]["status"] = status;

    // Include assigned_drives for recruiters
    std::string role = std::string(userDoc["role"].get_string().value);
    if (role == "recruiter" && userDoc.find("assigned_drives") != userDoc.end()) {
        Json::Value drives(Json::arrayValue);
        auto arr = userDoc["assigned_drives"].get_array().value;
        for (auto& driveId : arr) {
            if (driveId.type() == bsoncxx::type::k_string) {
                drives.append(std::string(driveId.get_string().value));
            }
        }
        result["user"]["assigned_drives"] = drives;
    }

    return result;
}
