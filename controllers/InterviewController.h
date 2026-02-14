#pragma once

#include <drogon/HttpController.h>

class InterviewController : public drogon::HttpController<InterviewController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(InterviewController::scheduleInterview, "/api/interviews", drogon::Post, "AuthFilter");
    ADD_METHOD_TO(InterviewController::getAllInterviews, "/api/interviews", drogon::Get, "AuthFilter");
    ADD_METHOD_TO(InterviewController::getMyInterviews, "/api/interviews/me", drogon::Get, "AuthFilter", "StudentFilter");
    METHOD_LIST_END

    void scheduleInterview(const drogon::HttpRequestPtr &req,
                           std::function<void(const drogon::HttpResponsePtr &)> &&callback);
    void getAllInterviews(const drogon::HttpRequestPtr &req,
                         std::function<void(const drogon::HttpResponsePtr &)> &&callback);
    void getMyInterviews(const drogon::HttpRequestPtr &req,
                         std::function<void(const drogon::HttpResponsePtr &)> &&callback);
};
