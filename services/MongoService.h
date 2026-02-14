#pragma once

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/pool.hpp>
#include <mongocxx/uri.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <memory>
#include <string>

class MongoService {
public:
    static MongoService& instance();

    void init(const std::string& uri, const std::string& dbName);
    void createIndexes();
    void seedTpo();

    // Acquire a client from the pool. Caller MUST keep the entry alive
    // for the entire duration they use any database/collection from it.
    mongocxx::pool::entry acquireClient();

    // Get a database handle from an acquired client
    mongocxx::database getDb(mongocxx::pool::entry& client);

    // Get a collection handle from an acquired client
    mongocxx::collection getCollection(mongocxx::pool::entry& client, const std::string& name);

    const std::string& dbName() const { return dbName_; }

private:
    MongoService() = default;
    MongoService(const MongoService&) = delete;
    MongoService& operator=(const MongoService&) = delete;

    static mongocxx::instance inst_;
    std::unique_ptr<mongocxx::pool> pool_;
    std::string dbName_;
};
