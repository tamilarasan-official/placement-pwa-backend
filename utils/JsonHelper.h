#pragma once

#include <json/json.h>
#include <bsoncxx/document/view.hpp>
#include <bsoncxx/document/value.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/types.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/oid.hpp>
#include <string>

class JsonHelper {
public:
    static Json::Value bsonToJson(bsoncxx::document::view doc);
    static bsoncxx::document::value jsonToBson(const Json::Value& json);
    static Json::Value parse(const std::string& str);
    static std::string stringify(const Json::Value& json);
    static Json::Value errorResponse(const std::string& message);
    static Json::Value successResponse(const std::string& message);
};
