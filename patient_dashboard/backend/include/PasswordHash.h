#pragma once
#include <string>
#include <random>
#include <sstream>
#include <iomanip>
#include <openssl/sha.h>

// Salted SHA-256 password hashing. This is a course-project demo, not a
// production auth system (a real system would use bcrypt/argon2 with a
// tunable work factor) -- but it means passwords are never persisted or
// compared in plaintext.
namespace PasswordHash {

inline std::string sha256Hex(const std::string& data) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(data.data()), data.size(), digest);
    std::ostringstream oss;
    for (unsigned char b : digest) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    }
    return oss.str();
}

// 16 random bytes, hex-encoded (32 hex chars). std::random_device is used
// directly rather than seeding a PRNG -- this only needs to be unique per
// user, not cryptographically unpredictable, so its (platform-dependent)
// entropy quality is good enough here.
inline std::string randomSaltHex() {
    std::random_device rd;
    std::uniform_int_distribution<int> byteDist(0, 255);
    std::ostringstream oss;
    for (int i = 0; i < 16; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << byteDist(rd);
    }
    return oss.str();
}

inline std::string hash(const std::string& password, const std::string& saltHex) {
    return sha256Hex(saltHex + password);
}

} // namespace PasswordHash
