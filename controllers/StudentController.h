#pragma once

#include <drogon/HttpController.h>

class StudentController : public drogon::HttpController<StudentController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(StudentController::getProfile, "/api/students/profile", drogon::Get, "AuthFilter", "StudentFilter");
    ADD_METHOD_TO(StudentController::updateProfile, "/api/students/profile", drogon::Put, "AuthFilter", "StudentFilter");
    ADD_METHOD_TO(StudentController::uploadResume, "/api/students/resume", drogon::Post, "AuthFilter", "StudentFilter");
    ADD_METHOD_TO(StudentController::getEligibleDrives, "/api/students/eligible-drives", drogon::Get, "AuthFilter", "StudentFilter");
    ADD_METHOD_TO(StudentController::getRecommendedDrives, "/api/students/recommended-drives", drogon::Get, "AuthFilter", "StudentFilter");
    ADD_METHOD_TO(StudentController::applyToDrive, "/api/students/apply", drogon::Post, "AuthFilter", "StudentFilter");
    ADD_METHOD_TO(StudentController::getApplications, "/api/students/applications", drogon::Get, "AuthFilter", "StudentFilter");
    METHOD_LIST_END

    void getProfile(const drogon::HttpRequestPtr &req,
                    std::function<void(const drogon::HttpResponsePtr &)> &&callback);
    void updateProfile(const drogon::HttpRequestPtr &req,
                       std::function<void(const drogon::HttpResponsePtr &)> &&callback);
    void uploadResume(const drogon::HttpRequestPtr &req,
                      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
    void getEligibleDrives(const drogon::HttpRequestPtr &req,
                           std::function<void(const drogon::HttpResponsePtr &)> &&callback);
    void getRecommendedDrives(const drogon::HttpRequestPtr &req,
                              std::function<void(const drogon::HttpResponsePtr &)> &&callback);
    void applyToDrive(const drogon::HttpRequestPtr &req,
                      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
    void getApplications(const drogon::HttpRequestPtr &req,
                         std::function<void(const drogon::HttpResponsePtr &)> &&callback);
};
