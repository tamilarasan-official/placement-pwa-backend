#include "AuthFilter.h"
#include "JwtHelper.h"
#include "MongoService.h"
#include <drogon/HttpResponse.h>
#include <json/json.h>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/oid.hpp>

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;

void AuthFilter::doFilter(const drogon::HttpRequestPtr &req,
                           drogon::FilterCallback &&cb,
                           drogon::FilterChainCallback &&ccb) {
    std::string authHeader = req->getHeader("Authorization");

    if (authHeader.empty() || authHeader.substr(0, 7) != "Bearer ") {
        Json::Value err;
        err["success"] = false;
        err["error"] = "No authentication token provided";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
        resp->setStatusCode(drogon::k401Unauthorized);
        cb(resp);
        return;
    }

    std::string token = authHeader.substr(7);
    Json::Value payload = JwtHelper::verifyToken(token);

    if (payload.isMember("error")) {
        Json::Value err;
        err["success"] = false;
        err["error"] = payload["error"].asString();
        auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
        resp->setStatusCode(drogon::k401Unauthorized);
        cb(resp);
        return;
    }

    std::string userId = payload["user_id"].asString();
    std::string role = payload["role"].asString();

    // Verify user still has active status in database
    try {
        auto client = MongoService::instance().acquireClient();
        auto users = MongoService::instance().getCollection(client, "users");
        auto userOpt = users.find_one(
            make_document(kvp("_id", bsoncxx::oid{userId}))
        );

        if (!userOpt) {
            Json::Value err;
            err["success"] = false;
            err["error"] = "User account not found";
            auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
            resp->setStatusCode(drogon::k401Unauthorized);
            cb(resp);
            return;
        }

        auto userDoc = userOpt->view();
        std::string status = "active";
        if (userDoc.find("status") != userDoc.end()) {
            status = std::string(userDoc["status"].get_string().value);
        }

        if (status != "active") {
            Json::Value err;
            err["success"] = false;
            err["error"] = "Account is no longer active";
            err["status"] = status;
            auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
            resp->setStatusCode(drogon::k403Forbidden);
            cb(resp);
            return;
        }
    } catch (const std::exception& e) {
        Json::Value err;
        err["success"] = false;
        err["error"] = "Authentication verification failed";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
        resp->setStatusCode(drogon::k500InternalServerError);
        cb(resp);
        return;
    }

    // Attach user info to request attributes
    req->attributes()->insert("user_id", userId);
    req->attributes()->insert("role", role);

    ccb();
}
