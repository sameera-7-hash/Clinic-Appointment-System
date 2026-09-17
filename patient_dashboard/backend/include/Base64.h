#pragma once
#include <string>
#include <openssl/evp.h>
#include <vector>
#include <stdexcept>

// Small base64 helper so binary IV/ciphertext bytes can be stored safely
// inside JSON text. Wraps OpenSSL's own base64 primitives rather than
// hand-rolling encoding — one less place for a subtle bug to hide.
namespace base64 {

inline std::string encode(const std::string& raw) {
    if (raw.empty()) return "";
    std::vector<unsigned char> out(4 * ((raw.size() + 2) / 3) + 1);
    int len = EVP_EncodeBlock(out.data(),
                               reinterpret_cast<const unsigned char*>(raw.data()),
                               static_cast<int>(raw.size()));
    return std::string(reinterpret_cast<char*>(out.data()), len);
}

inline std::string decode(const std::string& encoded) {
    if (encoded.empty()) return "";
    std::vector<unsigned char> out(3 * (encoded.size() / 4) + 3);
    int len = EVP_DecodeBlock(out.data(),
                               reinterpret_cast<const unsigned char*>(encoded.data()),
                               static_cast<int>(encoded.size()));
    if (len < 0) throw std::runtime_error("base64 decode failed");
    // EVP_DecodeBlock does not account for padding ('=') itself, trim it.
    size_t padding = 0;
    if (encoded.size() >= 2) {
        if (encoded[encoded.size() - 1] == '=') padding++;
        if (encoded[encoded.size() - 2] == '=') padding++;
    }
    return std::string(reinterpret_cast<char*>(out.data()), len - padding);
}

} // namespace base64
