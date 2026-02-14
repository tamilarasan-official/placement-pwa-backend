#pragma once

#include <json/json.h>
#include <string>

class PlacementService {
public:
    static bool isValidStatusTransition(const std::string& current, const std::string& next);
    static Json::Value getAnalytics();
    static Json::Value createNotification(const std::string& userId, const std::string& message, const std::string& type);
};
