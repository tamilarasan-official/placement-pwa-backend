#include <drogon/drogon.h>
#include "MongoService.h"
#include <iostream>
#include <cstdlib>

int main() {
    // Initialize MongoDB
    const char* mongoUri = std::getenv("MONGO_URI");
    const char* dbName = std::getenv("MONGO_DB");

    if (!mongoUri) {
        mongoUri = "mongodb+srv://thamizhlogesh2020_db:CznmXjccKjETfBNL@cluster0.eq96sdn.mongodb.net/?appName=Cluster0";
    }
    if (!dbName) {
        dbName = "placementdb";
    }

    MongoService::instance().init(mongoUri, dbName);
    std::cout << "MongoDB initialized successfully" << std::endl;

    // Configure Drogon
    auto &app = drogon::app();

    // Load config
    app.loadConfigFile("./config.json");

    // CORS middleware
    app.registerPreRoutingAdvice([](const drogon::HttpRequestPtr &req,
                                     drogon::FilterCallback &&stop,
                                     drogon::FilterChainCallback &&pass) {
        if (req->method() == drogon::Options) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->addHeader("Access-Control-Allow-Origin", "*");
            resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
            resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
            resp->addHeader("Access-Control-Max-Age", "86400");
            stop(resp);
            return;
        }
        pass();
    });

    app.registerPostHandlingAdvice([](const drogon::HttpRequestPtr &req,
                                       const drogon::HttpResponsePtr &resp) {
        resp->addHeader("Access-Control-Allow-Origin", "*");
        resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
    });

    // Create indexes
    MongoService::instance().createIndexes();

    // Auto-seed TPO account if none exists
    MongoService::instance().seedTpo();

    std::cout << "Server starting on port 8080..." << std::endl;
    app.run();

    return 0;
}
