#pragma once

#include <string>
#include <vector>
#include <openssl/rand.h>
#include <openssl/evp.h>

class BcryptHelper {
public:
    static std::string hashPassword(const std::string& password);
    static bool verifyPassword(const std::string& password, const std::string& hash);
private:
    static std::string generateSalt(int length = 16);
    static std::string pbkdf2Hash(const std::string& password, const std::string& salt, int iterations = 10000);
    static std::string toHex(const unsigned char* data, int len);
    static std::vector<unsigned char> fromHex(const std::string& hex);
};
