#pragma once

#include <drogon/HttpController.h>

class CompanyController : public drogon::HttpController<CompanyController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(CompanyController::createCompany, "/api/companies", drogon::Post, "AuthFilter", "TpoFilter");
    ADD_METHOD_TO(CompanyController::getAllCompanies, "/api/companies", drogon::Get, "AuthFilter");
    ADD_METHOD_TO(CompanyController::getCompany, "/api/companies/{id}", drogon::Get, "AuthFilter");
    ADD_METHOD_TO(CompanyController::updateCompany, "/api/companies/{id}", drogon::Put, "AuthFilter", "TpoFilter");
    ADD_METHOD_TO(CompanyController::deleteCompany, "/api/companies/{id}", drogon::Delete, "AuthFilter", "TpoFilter");
    ADD_METHOD_TO(CompanyController::getEligibleStudents, "/api/companies/{id}/eligible-students", drogon::Get, "AuthFilter");
    ADD_METHOD_TO(CompanyController::getDriveApplications, "/api/companies/{id}/applications", drogon::Get, "AuthFilter");
    METHOD_LIST_END

    void createCompany(const drogon::HttpRequestPtr &req,
                       std::function<void(const drogon::HttpResponsePtr &)> &&callback);
    void getAllCompanies(const drogon::HttpRequestPtr &req,
                        std::function<void(const drogon::HttpResponsePtr &)> &&callback);
    void getCompany(const drogon::HttpRequestPtr &req,
                    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                    const std::string &id);
    void updateCompany(const drogon::HttpRequestPtr &req,
                       std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                       const std::string &id);
    void deleteCompany(const drogon::HttpRequestPtr &req,
                       std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                       const std::string &id);
    void getEligibleStudents(const drogon::HttpRequestPtr &req,
                             std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                             const std::string &id);
    void getDriveApplications(const drogon::HttpRequestPtr &req,
                              std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                              const std::string &id);
};
