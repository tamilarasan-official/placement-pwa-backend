#include "JwtHelper.h"
#include <openssl/hmac.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <ctime>
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <vector>

namespace {

std::string base64UrlEncode(const std::string& input) {
    BIO *bio, *b64;
    BUF_MEM *bufferPtr;
    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, input.c_str(), static_cast<int>(input.length()));
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &bufferPtr);
    std::string result(bufferPtr->data, bufferPtr->length);
    BIO_free_all(bio);
    // Convert to URL-safe base64
    for (auto& c : result) {
        if (c == '+') c = '-';
        else if (c == '/') c = '_';
    }
    // Remove padding
    while (!result.empty() && result.back() == '=') {
        result.pop_back();
    }
    return result;
}

std::string base64UrlDecode(const std::string& input) {
    std::string padded = input;
    // Convert from URL-safe
    for (auto& c : padded) {
        if (c == '-') c = '+';
        else if (c == '_') c = '/';
    }
    // Add padding
    while (padded.size() % 4 != 0) {
        padded += '=';
    }

    BIO *bio, *b64;
    std::vector<char> buffer(padded.size());
    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new_mem_buf(padded.c_str(), static_cast<int>(padded.size()));
    bio = BIO_push(b64, bio);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    int len = BIO_read(bio, buffer.data(), static_cast<int>(buffer.size()));
    BIO_free_all(bio);
    return std::string(buffer.data(), len > 0 ? len : 0);
}

std::string hmacSha256(const std::string& data, const std::string& key) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hashLen;
    HMAC(EVP_sha256(),
         key.c_str(), static_cast<int>(key.length()),
         reinterpret_cast<const unsigned char*>(data.c_str()),
         data.length(),
         hash, &hashLen);
    return std::string(reinterpret_cast<char*>(hash), hashLen);
}

} // anonymous namespace

std::string JwtHelper::getSecret() {
    const char* secret = std::getenv("JWT_SECRET");
    if (secret) return std::string(secret);
    return "placement_system_jwt_secret_key_2026";
}

std::string JwtHelper::generateToken(const std::string& userId, const std::string& role) {
    // Header
    Json::Value header;
    header["alg"] = "HS256";
    header["typ"] = "JWT";

    // Payload
    Json::Value payload;
    payload["user_id"] = userId;
    payload["role"] = role;
    payload["iat"] = static_cast<Json::Int64>(std::time(nullptr));
    payload["exp"] = static_cast<Json::Int64>(std::time(nullptr) + 86400 * 7); // 7 days

    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    std::string headerStr = Json::writeString(writer, header);
    std::string payloadStr = Json::writeString(writer, payload);

    std::string headerB64 = base64UrlEncode(headerStr);
    std::string payloadB64 = base64UrlEncode(payloadStr);

    std::string sigInput = headerB64 + "." + payloadB64;
    std::string signature = base64UrlEncode(hmacSha256(sigInput, getSecret()));

    return sigInput + "." + signature;
}

Json::Value JwtHelper::verifyToken(const std::string& token) {
    Json::Value result;

    // Split token into parts
    std::vector<std::string> parts;
    std::stringstream ss(token);
    std::string part;
    while (std::getline(ss, part, '.')) {
        parts.push_back(part);
    }

    if (parts.size() != 3) {
        result["error"] = "Invalid token format";
        return result;
    }

    // Verify signature
    std::string sigInput = parts[0] + "." + parts[1];
    std::string expectedSig = base64UrlEncode(hmacSha256(sigInput, getSecret()));

    if (expectedSig != parts[2]) {
        result["error"] = "Invalid signature";
        return result;
    }

    // Decode payload
    std::string payloadStr = base64UrlDecode(parts[1]);

    Json::CharReaderBuilder reader;
    std::istringstream payloadStream(payloadStr);
    std::string errors;
    Json::Value payload;
    if (!Json::parseFromStream(reader, payloadStream, &payload, &errors)) {
        result["error"] = "Failed to parse payload";
        return result;
    }

    // Check expiration
    auto now = static_cast<Json::Int64>(std::time(nullptr));
    if (payload.isMember("exp") && payload["exp"].asInt64() < now) {
        result["error"] = "Token expired";
        return result;
    }

    return payload;
}
