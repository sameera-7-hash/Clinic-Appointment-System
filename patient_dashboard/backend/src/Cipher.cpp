#include "Cipher.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/provider.h>
#include <stdexcept>
#include <vector>
#include <cstring>

namespace {

// OpenSSL 3.x split classic algorithms it now considers obsolete —
// including single-key DES — out of the "default" provider and into a
// separate "legacy" provider that isn't loaded automatically. Without
// loading it, EVP_des_cbc() is a valid pointer but EVP_EncryptInit_ex
// fails at runtime because the actual implementation isn't registered.
// This runs once, the first time either cipher is used.
void ensureLegacyProviderLoaded() {
    static bool loaded = []() {
        OSSL_PROVIDER_load(nullptr, "legacy");
        OSSL_PROVIDER_load(nullptr, "default");
        return true;
    }();
    (void)loaded;
}

// Turns an arbitrary-length human passphrase into a fixed-length key by
// hashing it with SHA-256 and taking the first keyLen bytes. This is a
// simple key derivation, not a hardened one (a real system would use
// PBKDF2/scrypt/Argon2 with a per-record salt) — noted here so it isn't
// mistaken for one.
std::string deriveKey(const std::string& passphrase, size_t keyLen) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(passphrase.data()), passphrase.size(), digest);
    if (keyLen > SHA256_DIGEST_LENGTH) {
        throw std::invalid_argument("requested key length exceeds SHA-256 output");
    }
    return std::string(reinterpret_cast<char*>(digest), keyLen);
}

// Generic EVP-based CBC encrypt used by both AES and DES below. Handles
// PKCS#7 padding via OpenSSL's default behavior (left enabled).
std::string evpEncrypt(const EVP_CIPHER* algo, const std::string& plaintext,
                        const std::string& passphrase, size_t keyLen, size_t ivLen) {
    ensureLegacyProviderLoaded();
    std::string key = deriveKey(passphrase, keyLen);
    std::vector<unsigned char> iv(ivLen);
    if (RAND_bytes(iv.data(), static_cast<int>(ivLen)) != 1) {
        throw std::runtime_error("RAND_bytes failed while generating IV");
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throw std::runtime_error("EVP_CIPHER_CTX_new failed");

    std::string result;
    try {
        if (EVP_EncryptInit_ex(ctx, algo, nullptr,
                                reinterpret_cast<const unsigned char*>(key.data()),
                                iv.data()) != 1) {
            throw std::runtime_error("EVP_EncryptInit_ex failed");
        }

        std::vector<unsigned char> outBuf(plaintext.size() + EVP_MAX_BLOCK_LENGTH);
        int outLen1 = 0;
        if (EVP_EncryptUpdate(ctx, outBuf.data(), &outLen1,
                               reinterpret_cast<const unsigned char*>(plaintext.data()),
                               static_cast<int>(plaintext.size())) != 1) {
            throw std::runtime_error("EVP_EncryptUpdate failed");
        }
        int outLen2 = 0;
        if (EVP_EncryptFinal_ex(ctx, outBuf.data() + outLen1, &outLen2) != 1) {
            throw std::runtime_error("EVP_EncryptFinal_ex failed");
        }

        result.reserve(ivLen + outLen1 + outLen2);
        result.append(reinterpret_cast<char*>(iv.data()), ivLen);
        result.append(reinterpret_cast<char*>(outBuf.data()), outLen1 + outLen2);
    } catch (...) {
        EVP_CIPHER_CTX_free(ctx);
        throw;
    }
    EVP_CIPHER_CTX_free(ctx);
    return result;
}

std::string evpDecrypt(const EVP_CIPHER* algo, const std::string& ivAndCiphertext,
                        const std::string& passphrase, size_t keyLen, size_t ivLen) {
    ensureLegacyProviderLoaded();
    if (ivAndCiphertext.size() < ivLen) {
        throw std::runtime_error("ciphertext shorter than IV, cannot decrypt");
    }
    std::string key = deriveKey(passphrase, keyLen);
    const unsigned char* iv = reinterpret_cast<const unsigned char*>(ivAndCiphertext.data());
    const unsigned char* cipherBytes = iv + ivLen;
    size_t cipherLen = ivAndCiphertext.size() - ivLen;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throw std::runtime_error("EVP_CIPHER_CTX_new failed");

    std::string result;
    try {
        if (EVP_DecryptInit_ex(ctx, algo, nullptr,
                                reinterpret_cast<const unsigned char*>(key.data()), iv) != 1) {
            throw std::runtime_error("EVP_DecryptInit_ex failed");
        }

        std::vector<unsigned char> outBuf(cipherLen + EVP_MAX_BLOCK_LENGTH);
        int outLen1 = 0;
        if (EVP_DecryptUpdate(ctx, outBuf.data(), &outLen1, cipherBytes,
                               static_cast<int>(cipherLen)) != 1) {
            throw std::runtime_error("EVP_DecryptUpdate failed");
        }
        int outLen2 = 0;
        // A wrong key/passphrase or corrupted ciphertext most often shows up
        // right here as a padding failure — this is the "integrity" signal
        // PatientStorage relies on in addition to the explicit hash chain.
        if (EVP_DecryptFinal_ex(ctx, outBuf.data() + outLen1, &outLen2) != 1) {
            throw std::runtime_error("decryption failed (wrong key or corrupted data)");
        }
        result.assign(reinterpret_cast<char*>(outBuf.data()), outLen1 + outLen2);
    } catch (...) {
        EVP_CIPHER_CTX_free(ctx);
        throw;
    }
    EVP_CIPHER_CTX_free(ctx);
    return result;
}

} // namespace

std::string AesCipher::encrypt(const std::string& plaintext, const std::string& passphrase) const {
    return evpEncrypt(EVP_aes_256_cbc(), plaintext, passphrase, 32, 16);
}
std::string AesCipher::decrypt(const std::string& ivAndCiphertext, const std::string& passphrase) const {
    return evpDecrypt(EVP_aes_256_cbc(), ivAndCiphertext, passphrase, 32, 16);
}

std::string DesCipher::encrypt(const std::string& plaintext, const std::string& passphrase) const {
    return evpEncrypt(EVP_des_cbc(), plaintext, passphrase, 8, 8);
}
std::string DesCipher::decrypt(const std::string& ivAndCiphertext, const std::string& passphrase) const {
    return evpDecrypt(EVP_des_cbc(), ivAndCiphertext, passphrase, 8, 8);
}

std::unique_ptr<CipherStrategy> cipherFor(const std::string& algoName) {
    if (algoName == "AES-256-CBC") return std::make_unique<AesCipher>();
    if (algoName == "DES-CBC") return std::make_unique<DesCipher>();
    throw std::invalid_argument("unknown cipher algorithm: " + algoName);
}
