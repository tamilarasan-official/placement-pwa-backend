#pragma once

#include <drogon/HttpController.h>

class TpoController : public drogon::HttpController<TpoController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(TpoController::getPendingStudents, "/api/tpo/pending-students", drogon::Get, "AuthFilter", "TpoFilter");
    ADD_METHOD_TO(TpoController::approveStudent, "/api/tpo/students/{id}/approve", drogon::Put, "AuthFilter", "TpoFilter");
    ADD_METHOD_TO(TpoController::rejectStudent, "/api/tpo/students/{id}/reject", drogon::Put, "AuthFilter", "TpoFilter");
    ADD_METHOD_TO(TpoController::getRecruiters, "/api/tpo/recruiters", drogon::Get, "AuthFilter", "TpoFilter");
    METHOD_LIST_END

    void getPendingStudents(const drogon::HttpRequestPtr &req,
                            std::function<void(const drogon::HttpResponsePtr &)> &&callback);
    void approveStudent(const drogon::HttpRequestPtr &req,
                        std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                        const std::string &id);
    void rejectStudent(const drogon::HttpRequestPtr &req,
                       std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                       const std::string &id);
    void getRecruiters(const drogon::HttpRequestPtr &req,
                       std::function<void(const drogon::HttpResponsePtr &)> &&callback);
};
