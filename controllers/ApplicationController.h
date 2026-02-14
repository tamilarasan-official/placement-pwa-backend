#pragma once

#include <drogon/HttpController.h>

class ApplicationController : public drogon::HttpController<ApplicationController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(ApplicationController::updateStatus, "/api/applications/{id}/status", drogon::Put, "AuthFilter");
    METHOD_LIST_END

    void updateStatus(const drogon::HttpRequestPtr &req,
                      std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                      const std::string &id);
};
