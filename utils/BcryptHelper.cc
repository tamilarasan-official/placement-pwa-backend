#include "BcryptHelper.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <sstream>
#include <iomanip>
#include <vector>
#include <cstring>

std::string BcryptHelper::toHex(const unsigned char* data, int len) {
    std::ostringstream oss;
    for (int i = 0; i < len; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)data[i];
    }
    return oss.str();
}

std::vector<unsigned char> BcryptHelper::fromHex(const std::string& hex) {
    std::vector<unsigned char> bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        unsigned char byte = static_cast<unsigned char>(
            std::stoi(hex.substr(i, 2), nullptr, 16));
        bytes.push_back(byte);
    }
    return bytes;
}

std::string BcryptHelper::generateSalt(int length) {
    std::vector<unsigned char> salt(length);
    RAND_bytes(salt.data(), length);
    return toHex(salt.data(), length);
}

std::string BcryptHelper::pbkdf2Hash(const std::string& password, const std::string& salt, int iterations) {
    unsigned char hash[32]; // SHA-256 output
    auto saltBytes = fromHex(salt);

    PKCS5_PBKDF2_HMAC(password.c_str(), static_cast<int>(password.length()),
                        saltBytes.data(), static_cast<int>(saltBytes.size()),
                        iterations, EVP_sha256(), 32, hash);

    return toHex(hash, 32);
}

std::string BcryptHelper::hashPassword(const std::string& password) {
    std::string salt = generateSalt(16);
    std::string hash = pbkdf2Hash(password, salt);
    // Format: iterations$salt$hash
    return "10000$" + salt + "$" + hash;
}

bool BcryptHelper::verifyPassword(const std::string& password, const std::string& storedHash) {
    // Parse stored hash: iterations$salt$hash
    size_t first = storedHash.find('$');
    size_t second = storedHash.find('$', first + 1);

    if (first == std::string::npos || second == std::string::npos) {
        return false;
    }

    int iterations = std::stoi(storedHash.substr(0, first));
    std::string salt = storedHash.substr(first + 1, second - first - 1);
    std::string hash = storedHash.substr(second + 1);

    std::string computedHash = pbkdf2Hash(password, salt, iterations);
    return computedHash == hash;
}
