#pragma once

#include <drogon/HttpController.h>

class AuthController : public drogon::HttpController<AuthController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(AuthController::registerUser, "/api/auth/register", drogon::Post);
    ADD_METHOD_TO(AuthController::loginUser, "/api/auth/login", drogon::Post);
    ADD_METHOD_TO(AuthController::getMe, "/api/auth/me", drogon::Get, "AuthFilter");
    METHOD_LIST_END

    void registerUser(const drogon::HttpRequestPtr &req,
                      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
    void loginUser(const drogon::HttpRequestPtr &req,
                   std::function<void(const drogon::HttpResponsePtr &)> &&callback);
    void getMe(const drogon::HttpRequestPtr &req,
               std::function<void(const drogon::HttpResponsePtr &)> &&callback);
};
