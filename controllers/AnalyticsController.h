#pragma once

#include <drogon/HttpController.h>

class AnalyticsController : public drogon::HttpController<AnalyticsController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(AnalyticsController::getAnalytics, "/api/analytics", drogon::Get, "AuthFilter", "TpoFilter");
    ADD_METHOD_TO(AnalyticsController::getNotifications, "/api/notifications", drogon::Get, "AuthFilter");
    ADD_METHOD_TO(AnalyticsController::markNotificationRead, "/api/notifications/{id}/read", drogon::Put, "AuthFilter");
    ADD_METHOD_TO(AnalyticsController::getAllStudents, "/api/students", drogon::Get, "AuthFilter", "TpoFilter");
    ADD_METHOD_TO(AnalyticsController::getAllApplications, "/api/applications", drogon::Get, "AuthFilter", "TpoFilter");
    METHOD_LIST_END

    void getAnalytics(const drogon::HttpRequestPtr &req,
                      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
    void getNotifications(const drogon::HttpRequestPtr &req,
                          std::function<void(const drogon::HttpResponsePtr &)> &&callback);
    void markNotificationRead(const drogon::HttpRequestPtr &req,
                              std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                              const std::string &id);
    void getAllStudents(const drogon::HttpRequestPtr &req,
                        std::function<void(const drogon::HttpResponsePtr &)> &&callback);
    void getAllApplications(const drogon::HttpRequestPtr &req,
                            std::function<void(const drogon::HttpResponsePtr &)> &&callback);
};
