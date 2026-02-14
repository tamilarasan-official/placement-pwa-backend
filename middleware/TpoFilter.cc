#include "TpoFilter.h"
#include <drogon/HttpResponse.h>
#include <json/json.h>

void TpoFilter::doFilter(const drogon::HttpRequestPtr &req,
                          drogon::FilterCallback &&cb,
                          drogon::FilterChainCallback &&ccb) {
    auto role = req->attributes()->get<std::string>("role");

    if (role != "tpo") {
        Json::Value err;
        err["success"] = false;
        err["error"] = "TPO access required";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
        resp->setStatusCode(drogon::k403Forbidden);
        cb(resp);
        return;
    }

    ccb();
}
