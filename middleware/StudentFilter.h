#pragma once

#include <drogon/HttpFilter.h>

class StudentFilter : public drogon::HttpFilter<StudentFilter> {
public:
    void doFilter(const drogon::HttpRequestPtr &req,
                  drogon::FilterCallback &&cb,
                  drogon::FilterChainCallback &&ccb) override;
};
