#include "JsonHelper.h"
#include <bsoncxx/types.hpp>
#include <sstream>

Json::Value JsonHelper::bsonToJson(bsoncxx::document::view doc) {
    // Use bsoncxx's built-in JSON conversion then parse
    std::string jsonStr = bsoncxx::to_json(doc);
    return parse(jsonStr);
}

bsoncxx::document::value JsonHelper::jsonToBson(const Json::Value& json) {
    std::string jsonStr = stringify(json);
    return bsoncxx::from_json(jsonStr);
}

Json::Value JsonHelper::parse(const std::string& str) {
    Json::Value root;
    Json::CharReaderBuilder reader;
    std::istringstream stream(str);
    std::string errors;
    Json::parseFromStream(reader, stream, &root, &errors);
    return root;
}

std::string JsonHelper::stringify(const Json::Value& json) {
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    return Json::writeString(writer, json);
}

Json::Value JsonHelper::errorResponse(const std::string& message) {
    Json::Value res;
    res["success"] = false;
    res["error"] = message;
    return res;
}

Json::Value JsonHelper::successResponse(const std::string& message) {
    Json::Value res;
    res["success"] = true;
    res["message"] = message;
    return res;
}
