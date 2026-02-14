#include "AuthController.h"
#include "AuthService.h"
#include "JsonHelper.h"

void AuthController::registerUser(const drogon::HttpRequestPtr &req,
                                   std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
    auto json = req->getJsonObject();
    if (!json) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse(
            JsonHelper::errorResponse("Invalid JSON body"));
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    auto result = AuthService::registerUser(*json);
    auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
    if (!result["success"].asBool()) {
        resp->setStatusCode(drogon::k400BadRequest);
    } else {
        // 201 Created for successful registration (no token returned)
        resp->setStatusCode(drogon::k201Created);
    }
    callback(resp);
}

void AuthController::loginUser(const drogon::HttpRequestPtr &req,
                                std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
    auto json = req->getJsonObject();
    if (!json) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse(
            JsonHelper::errorResponse("Invalid JSON body"));
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    auto result = AuthService::loginUser(*json);
    auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
    if (!result["success"].asBool()) {
        // Use 403 for status-based rejections, 401 for invalid credentials
        if (result.isMember("status")) {
            resp->setStatusCode(drogon::k403Forbidden);
        } else {
            resp->setStatusCode(drogon::k401Unauthorized);
        }
    }
    callback(resp);
}

void AuthController::getMe(const drogon::HttpRequestPtr &req,
                             std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
    auto userId = req->attributes()->get<std::string>("user_id");
    auto result = AuthService::getUserById(userId);
    auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
    if (!result["success"].asBool()) {
        resp->setStatusCode(drogon::k404NotFound);
    }
    callback(resp);
}
