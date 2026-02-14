#pragma once

#include <drogon/HttpFilter.h>

class RecruiterFilter : public drogon::HttpFilter<RecruiterFilter> {
public:
    void doFilter(const drogon::HttpRequestPtr &req,
                  drogon::FilterCallback &&cb,
                  drogon::FilterChainCallback &&ccb) override;
};
