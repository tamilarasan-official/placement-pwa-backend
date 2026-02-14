#include "MongoService.h"
#include "BcryptHelper.h"
#include <bsoncxx/builder/basic/array.hpp>
#include <iostream>
#include <chrono>

mongocxx::instance MongoService::inst_{};

MongoService& MongoService::instance() {
    static MongoService svc;
    return svc;
}

void MongoService::init(const std::string& uri, const std::string& dbName) {
    dbName_ = dbName;
    mongocxx::uri mongoUri{uri};
    pool_ = std::make_unique<mongocxx::pool>(mongoUri);

    // Test connection immediately
    try {
        auto client = pool_->acquire();
        auto db = (*client)[dbName_];
        // Ping the database to verify connectivity
        auto cmd = bsoncxx::builder::basic::make_document(
            bsoncxx::builder::basic::kvp("ping", 1));
        db.run_command(cmd.view());
        std::cout << "MongoDB connection verified - ping successful" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "MongoDB connection FAILED: " << e.what() << std::endl;
        throw;
    }
}

mongocxx::pool::entry MongoService::acquireClient() {
    return pool_->acquire();
}

mongocxx::database MongoService::getDb(mongocxx::pool::entry& client) {
    return (*client)[dbName_];
}

mongocxx::collection MongoService::getCollection(mongocxx::pool::entry& client, const std::string& name) {
    return (*client)[dbName_][name];
}

void MongoService::createIndexes() {
    using bsoncxx::builder::basic::kvp;
    using bsoncxx::builder::basic::make_document;

    try {
        auto client = acquireClient();

        // Users: unique email index + role/status compound
        {
            auto coll = getCollection(client, "users");
            mongocxx::options::index opts{};
            opts.unique(true);
            coll.create_index(make_document(kvp("email", 1)), opts);
            coll.create_index(make_document(kvp("role", 1), kvp("status", 1)));
        }

        // Students: unique user_id index + gpa/backlogs index
        {
            auto coll = getCollection(client, "students");
            mongocxx::options::index opts{};
            opts.unique(true);
            coll.create_index(make_document(kvp("user_id", 1)), opts);
            coll.create_index(make_document(kvp("gpa", 1), kvp("backlogs", 1)));
        }

        // Companies: recruiter_id + created_by indexes
        {
            auto coll = getCollection(client, "companies");
            coll.create_index(make_document(kvp("recruiter_id", 1)));
            coll.create_index(make_document(kvp("created_by", 1)));
        }

        // Applications: unique student_id + company_id, company_id index
        {
            auto coll = getCollection(client, "applications");
            mongocxx::options::index opts{};
            opts.unique(true);
            coll.create_index(make_document(kvp("student_id", 1), kvp("company_id", 1)), opts);
            coll.create_index(make_document(kvp("company_id", 1)));
        }

        // Interviews: student_id + company_id indexes
        {
            auto coll = getCollection(client, "interviews");
            coll.create_index(make_document(kvp("student_id", 1)));
            coll.create_index(make_document(kvp("company_id", 1)));
        }

        // Notifications: user_id + read, created_at descending
        {
            auto coll = getCollection(client, "notifications");
            coll.create_index(make_document(kvp("user_id", 1), kvp("read", 1)));
            coll.create_index(make_document(kvp("created_at", -1)));
        }

        std::cout << "Database indexes created successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Warning: Index creation: " << e.what() << std::endl;
    }
}

void MongoService::seedTpo() {
    using bsoncxx::builder::basic::kvp;
    using bsoncxx::builder::basic::make_document;
    using bsoncxx::builder::basic::make_array;

    try {
        auto client = acquireClient();
        auto users = getCollection(client, "users");

        // Check if any TPO user exists
        auto tpoOpt = users.find_one(make_document(kvp("role", "tpo")));
        if (tpoOpt) {
            std::cout << "TPO account already exists, skipping seed" << std::endl;
            return;
        }

        // Create the default TPO account
        std::string hashedPassword = BcryptHelper::hashPassword("Tpo@thamizh");
        auto now = bsoncxx::types::b_date{std::chrono::system_clock::now()};

        auto tpoDoc = make_document(
            kvp("name", "TPO Admin"),
            kvp("email", "thamizhdev@gmail.com"),
            kvp("password", hashedPassword),
            kvp("role", "tpo"),
            kvp("status", "active"),
            kvp("assigned_drives", make_array()),
            kvp("created_at", now)
        );

        users.insert_one(tpoDoc.view());
        std::cout << "TPO account seeded: thamizhdev@gmail.com" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Warning: TPO seed: " << e.what() << std::endl;
    }
}
