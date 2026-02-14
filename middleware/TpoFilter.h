#pragma once

#include <drogon/HttpFilter.h>

class TpoFilter : public drogon::HttpFilter<TpoFilter> {
public:
    void doFilter(const drogon::HttpRequestPtr &req,
                  drogon::FilterCallback &&cb,
                  drogon::FilterChainCallback &&ccb) override;
};
