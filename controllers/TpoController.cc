#include "TpoController.h"
#include "TpoService.h"
#include "JsonHelper.h"

void TpoController::getPendingStudents(const drogon::HttpRequestPtr &req,
                                        std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
    auto result = TpoService::getPendingStudents();
    callback(drogon::HttpResponse::newHttpJsonResponse(result));
}

void TpoController::approveStudent(const drogon::HttpRequestPtr &req,
                                    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                                    const std::string &id) {
    auto result = TpoService::approveStudent(id);
    auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
    if (!result["success"].asBool()) {
        resp->setStatusCode(drogon::k400BadRequest);
    }
    callback(resp);
}

void TpoController::rejectStudent(const drogon::HttpRequestPtr &req,
                                   std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                                   const std::string &id) {
    auto result = TpoService::rejectStudent(id);
    auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
    if (!result["success"].asBool()) {
        resp->setStatusCode(drogon::k400BadRequest);
    }
    callback(resp);
}

void TpoController::getRecruiters(const drogon::HttpRequestPtr &req,
                                   std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
    auto result = TpoService::getAllRecruiters();
    callback(drogon::HttpResponse::newHttpJsonResponse(result));
}
