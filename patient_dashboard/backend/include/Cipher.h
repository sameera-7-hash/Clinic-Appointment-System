#pragma once
#include <string>
#include <memory>

// Strategy pattern: the rest of the app talks to CipherStrategy and never
// knows or cares whether AES or DES is doing the work underneath. This is
// what makes it possible to store the algorithm name alongside each record
// and pick the right cipher back out again at read time, and to add a new
// algorithm later without touching PatientStorage or the API layer.
class CipherStrategy {
public:
    virtual ~CipherStrategy() = default;

    // Encrypts plaintext under a passphrase-derived key. A fresh random IV
    // is generated internally on every call and returned as part of the
    // output (format: raw bytes of IV, immediately followed by ciphertext).
    virtual std::string encrypt(const std::string& plaintext,
                                 const std::string& passphrase) const = 0;

    // Reverses encrypt(): expects [IV][ciphertext] as produced above.
    virtual std::string decrypt(const std::string& ivAndCiphertext,
                                 const std::string& passphrase) const = 0;

    // Name persisted alongside each record so it can be decrypted with the
    // same algorithm it was written with, e.g. "AES-256-CBC".
    virtual std::string name() const = 0;
};

// AES-256 in CBC mode. This is the algorithm that should actually protect
// real patient data — 128-bit block, 256-bit key, no known practical break.
class AesCipher : public CipherStrategy {
public:
    std::string encrypt(const std::string& plaintext, const std::string& passphrase) const override;
    std::string decrypt(const std::string& ivAndCiphertext, const std::string& passphrase) const override;
    std::string name() const override { return "AES-256-CBC"; }
};

// Classic single-key DES in CBC mode.
//
// IMPORTANT: DES has a 56-bit effective key length. It can be brute-forced
// with commodity/cloud hardware in well under a day, and has been
// considered broken for real-world confidentiality since the 1990s
// (see the EFF "Deep Crack" result, 1998). It is included here only
// because it was explicitly requested — for side-by-side comparison /
// teaching purposes (e.g. "encrypt this record with AES vs DES and see
// both the code and the ciphertext"). Do not use DesCipher for data you
// actually need to protect; use AesCipher.
class DesCipher : public CipherStrategy {
public:
    std::string encrypt(const std::string& plaintext, const std::string& passphrase) const override;
    std::string decrypt(const std::string& ivAndCiphertext, const std::string& passphrase) const override;
    std::string name() const override { return "DES-CBC"; }
};

// Factory: returns the cipher matching a name previously produced by
// CipherStrategy::name(), so a stored record can always be read back with
// the algorithm it was written with, regardless of which one is the
// current UI default. Throws std::invalid_argument for an unknown name.
std::unique_ptr<CipherStrategy> cipherFor(const std::string& algoName);
