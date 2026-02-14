#include <drogon/drogon.h>
#include "MongoService.h"
#include <iostream>
#include <cstdlib>
#include <vector>
#include <string>
#include <sstream>

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

    // Railway sets PORT env var — override config if present
    const char* portEnv = std::getenv("PORT");
    int port = portEnv ? std::stoi(portEnv) : 8080;
    app.addListener("0.0.0.0", port);
    app.setThreadNum(4);
    app.setLogLevel(trantor::Logger::kInfo);
    app.setUploadPath("./uploads");
    app.setClientMaxBodySize(10 * 1024 * 1024);  // 10MB

    // CORS configuration — supports multiple origins (comma-separated in env)
    const char* corsOriginEnv = std::getenv("CORS_ORIGIN");
    std::string corsEnv = corsOriginEnv ? corsOriginEnv : "https://campus-placement-tracking.vercel.app,https://placement-pwa-frontend-deploy.vercel.app";
    std::vector<std::string> allowedOrigins;
    {
        std::istringstream ss(corsEnv);
        std::string origin;
        while (std::getline(ss, origin, ',')) {
            allowedOrigins.push_back(origin);
        }
    }

    auto getMatchedOrigin = [allowedOrigins](const drogon::HttpRequestPtr &req) -> std::string {
        std::string origin = req->getHeader("Origin");
        for (const auto &allowed : allowedOrigins) {
            if (origin == allowed) return origin;
        }
        return allowedOrigins.empty() ? "*" : allowedOrigins[0];
    };

    app.registerPreRoutingAdvice([getMatchedOrigin](const drogon::HttpRequestPtr &req,
                                     drogon::FilterCallback &&stop,
                                     drogon::FilterChainCallback &&pass) {
        if (req->method() == drogon::Options) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->addHeader("Access-Control-Allow-Origin", getMatchedOrigin(req));
            resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
            resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
            resp->addHeader("Access-Control-Allow-Credentials", "true");
            resp->addHeader("Access-Control-Max-Age", "86400");
            stop(resp);
            return;
        }
        pass();
    });

    app.registerPostHandlingAdvice([getMatchedOrigin](const drogon::HttpRequestPtr &req,
                                       const drogon::HttpResponsePtr &resp) {
        resp->addHeader("Access-Control-Allow-Origin", getMatchedOrigin(req));
        resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
        resp->addHeader("Access-Control-Allow-Credentials", "true");
    });

    // Create indexes
    MongoService::instance().createIndexes();

    // Auto-seed TPO account if none exists
    MongoService::instance().seedTpo();

    std::cout << "Server starting on port " << port << "..." << std::endl;
    app.run();

    return 0;
}
