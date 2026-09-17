# Patient Records Dashboard — Full Build Guide

A C++ REST API that stores patient records **encrypted at rest** (AES-256 or DES,
your choice per record), appended to a **tamper-evident, hash-chained** file
with an automatic **backup mirror**, sitting behind a **login/session** layer,
with a plain HTML/JS **dashboard** talking to it over HTTP.

This README describes **every step**, in order, to go from an empty folder to
this exact running, tested application — every file's full contents, every
command, every test. Follow it top to bottom to build the whole thing by hand,
or jump to [Part 3](#part-3--build-and-run) if the code is already in front of
you and you just want to run it.

**Contents**
- [Part 1 — Prerequisites](#part-1--prerequisites)
- [Part 2 — Build every file, in order](#part-2--build-every-file-in-order)
- [Part 3 — Build and run](#part-3--build-and-run)
- [Part 4 — Use the dashboard](#part-4--use-the-dashboard)
- [Part 5 — Verify it actually works](#part-5--verify-it-actually-works)
- [API reference](#api-reference)
- [Environment variables reference](#environment-variables-reference)
- [Known limitations / next steps](#known-limitations--next-steps)

---

## Part 1 — Prerequisites

### Step 1 — Install the toolchain

You need a C++17 compiler, CMake, and OpenSSL's development headers (the
compiled crypto library plus the `.h` files describing it).

```bash
# Debian/Ubuntu
sudo apt install build-essential cmake libssl-dev

# macOS (Homebrew)
brew install cmake openssl

# Fedora/RHEL
sudo dnf install gcc-c++ cmake openssl-devel
```

### Step 2 — Confirm they're actually installed

```bash
which g++ cmake openssl
find / -iname "openssl/evp.h" 2>/dev/null | head -5
```

If the last command prints nothing, the OpenSSL *headers* (not just the
runtime library) aren't installed — reinstall the `-dev`/`-devel` package.

---

## Part 2 — Build every file, in order

Each file below depends only on the ones before it. Build them in this
sequence and you can compile-check your progress after every single step
(once `main.cpp` exists) instead of writing everything blind and debugging
it all at once.

### Step 3 — Create the folder structure

```bash
mkdir -p patient_dashboard/backend/include/third_party
mkdir -p patient_dashboard/backend/src
mkdir -p patient_dashboard/frontend
cd patient_dashboard
```

```
patient_dashboard/
├── backend/
│   ├── include/            headers — class declarations
│   │   └── third_party/    vendored single-header libraries
│   └── src/                 .cpp implementation files
└── frontend/
    └── index.html
```

### Step 4 — Fetch the two third-party libraries

Neither of these needs separate compiling — each is one header file you
`#include` directly.

```bash
cd backend/include/third_party
curl -fsSL -o httplib.h https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h
curl -fsSL -o json.hpp   https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp
cd ../../..
```

| Library | What it gives you |
|---|---|
| `httplib.h` ([cpp-httplib](https://github.com/yhirose/cpp-httplib)) | An embeddable HTTP server — `svr.Get(...)`, `svr.Post(...)`, routing, headers, everything `main.cpp` needs to be a web server |
| `json.hpp` ([nlohmann/json](https://github.com/nlohmann/json)) | C++ has no built-in JSON type; this turns a C++ object into JSON text and back |

### Step 5 — The data class: `Patient`

Deliberately dumb: fields, and two functions to convert to/from JSON.
It knows nothing about encryption, files, or HTTP — every other class
either produces one of these or consumes one. This is the
single-responsibility principle in practice: one class, one job.

Create `backend/include/Patient.h`:

```cpp
#pragma once
#include <string>
#include "third_party/json.hpp"

// Plain-old data class representing one patient record. Kept deliberately
// simple (a "data class") — encryption and persistence are handled by
// separate classes (CipherStrategy, PatientStorage) so each class has one
// job (single-responsibility), which is the point being demonstrated for
// an OOP project.
struct Patient {
    std::string id;             // generated, e.g. "P-000001"
    std::string name;
    int age = 0;
    std::string gender;
    std::string contact;
    std::string diagnosis;
    std::string admissionDate;  // ISO date string, e.g. "2026-09-02"

    nlohmann::json toJson() const {
        return nlohmann::json{
            {"id", id},
            {"name", name},
            {"age", age},
            {"gender", gender},
            {"contact", contact},
            {"diagnosis", diagnosis},
            {"admissionDate", admissionDate}
        };
    }

    static Patient fromJson(const nlohmann::json& j) {
        Patient p;
        p.id = j.value("id", "");
        p.name = j.value("name", "");
        p.age = j.value("age", 0);
        p.gender = j.value("gender", "");
        p.contact = j.value("contact", "");
        p.diagnosis = j.value("diagnosis", "");
        p.admissionDate = j.value("admissionDate", "");
        return p;
    }
};
```

### Step 6 — The base64 helper

Encrypted bytes are binary — they can't sit safely inside JSON text as-is.
Base64 turns arbitrary bytes into plain ASCII so they can. This wraps
OpenSSL's own base64 primitives instead of hand-rolling encoding, on the
same principle as Step 7 below: one less place for a subtle bug to hide.

Create `backend/include/Base64.h`:

```cpp
#pragma once
#include <string>
#include <openssl/evp.h>
#include <vector>
#include <stdexcept>

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
```

### Step 7 — The encryption strategy classes: `Cipher.h` / `Cipher.cpp`

One abstract interface (`CipherStrategy`), two implementations
(`AesCipher`, `DesCipher`) — the *Strategy pattern*. `PatientStorage`
(Step 9) will only ever talk to the interface, never to AES or DES
directly, so a third algorithm could be added later without touching
storage or API code at all.

**AES-256-CBC** is what should protect real data: a 256-bit key (2²⁵⁶
possibilities — not brute-forceable with any technology that exists).
**DES-CBC** is included because it was asked for, but it is genuinely
broken: only a 56-bit effective key, crackable in hours on rented cloud
hardware today. It's wired up as a real, working second option purely so
you can compare the code path and the resulting ciphertext side by side —
never use it to protect data you care about.

Both are **block ciphers** in **CBC mode**: they encrypt fixed-size chunks
(16 bytes for AES, 8 for DES), each chunk XORed with the previous
ciphertext chunk before encrypting, chained together so identical
plaintext blocks never produce identical ciphertext. That chain needs a
random starting point — the **IV** (Initialization Vector), regenerated on
every single call and stored right alongside the ciphertext (it isn't
secret, it just has to be unique per encryption).

Create `backend/include/Cipher.h`:

```cpp
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
```

Create `backend/src/Cipher.cpp`:

```cpp
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
```

> **Real bug you'll hit if you type this out yourself:** without
> `ensureLegacyProviderLoaded()`, `DesCipher::encrypt` compiles fine and
> fails at runtime with `EVP_EncryptInit_ex failed` on OpenSSL 3.x. This
> was discovered by actually running the code, not by reading OpenSSL's
> docs in advance — worth knowing before you spend an hour confused by it.

### Step 8 — The OOP contract for storage: `Storage.h`

Declares `PatientStorage` and the small result structs the API layer reads.
Written before `Storage.cpp` so the class's public contract — what it
promises to do — is settled before the internals are.

Create `backend/include/Storage.h`:

```cpp
#pragma once
#include <string>
#include <vector>
#include <mutex>
#include "Patient.h"
#include "Cipher.h"

// One decrypted record plus the bookkeeping the API/frontend need to show
// whether it came through the integrity check cleanly.
struct LoadedRecord {
    Patient patient;
    std::string algo;
    bool ok = true;          // false if this record failed hash/decrypt checks
    std::string problem;     // human-readable reason when ok == false
};

struct LoadResult {
    std::vector<LoadedRecord> records;
    bool chainIntact = true;
    bool usedBackup = false;      // true if primary file was unreadable and backup was used instead
};

// Append-only, encrypted, tamper-evident store for patient records.
//
// On disk, each record is one JSON line in patients.dat:
//   {"algo": "...", "iv_ct": "<base64 of IV+ciphertext>", "chain_hash": "<hex>"}
//
// chain_hash = SHA256(previous chain_hash + this record's iv_ct bytes).
// That makes the file a minimal hash chain (the same idea behind a
// tamper-evident ledger): editing, deleting, or reordering any past line
// changes its chain_hash, which no longer matches what the *next* line
// says came before it, so tampering anywhere in the file's history is
// detectable by recomputing the chain — not just checking one record.
//
// Durability: every successful append is written to patients.dat and then
// mirrored to patients.dat.bak before the call returns, so a single
// corrupted/truncated file (crash mid-write, disk error, accidental edit)
// can be recovered from the other copy. snapshotBackup() additionally
// makes a timestamped full-file copy on request.
class PatientStorage {
public:
    PatientStorage(std::string primaryPath, std::string backupPath, std::string passphrase);

    // Encrypts `patient` with `cipher` and appends it to the chain.
    // Thread-safe (guarded by an internal mutex) since the HTTP server
    // handles requests concurrently.
    void appendPatient(const Patient& patient, const CipherStrategy& cipher);

    // Decrypts and returns every record, verifying the hash chain and
    // falling back to the backup file if the primary is missing/corrupt.
    LoadResult loadAll(const std::string& passphrase) const;

    // Makes a timestamped snapshot copy of the primary file, e.g.
    // patients.dat.20260902T153000.bak
    std::string snapshotBackup() const;

    // Number of records currently stored (used to mint the next patient
    // ID) without paying the cost of decrypting every record.
    size_t recordCount() const;

private:
    std::string primaryPath_;
    std::string backupPath_;
    std::string passphrase_;
    mutable std::mutex mutex_;

    // Reads whichever file parses as a complete, valid file (primary
    // preferred); returns raw lines and whether backup had to be used.
    std::vector<std::string> readUsableLines(bool& usedBackup) const;

    std::string currentChainTip() const; // last chain_hash in the file, or "" if empty
};
```

### Step 9 — The storage engine: `Storage.cpp`

Build this in three passes, in order — each one only makes sense once the
last is working:

1. **Append + read, no verification.** Just get bytes onto disk and back.
2. **Add `chain_hash`, verify it on read.** This is the tamper-evidence.
3. **Add the dual-write to `.bak` and the fallback-on-read logic.** This
   depends on (1) and (2) already working, since it has to decide which of
   two *already-parseable* files to trust.

Create `backend/src/Storage.cpp`:

```cpp
#include "Storage.h"
#include "Base64.h"
#include "third_party/json.hpp"
#include <openssl/sha.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <filesystem>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {

std::string sha256Hex(const std::string& data) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(data.data()), data.size(), digest);
    std::ostringstream oss;
    for (unsigned char b : digest) oss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
    return oss.str();
}

// chain_hash for a record = SHA256(previous record's chain_hash + this
// record's base64 iv+ciphertext). Linking each record to the one before
// it is what makes a change to an old record detectable later.
std::string chainHash(const std::string& prevHash, const std::string& ivCtBase64) {
    return sha256Hex(prevHash + ivCtBase64);
}

std::vector<std::string> splitLines(const std::string& content) {
    std::vector<std::string> lines;
    std::istringstream iss(content);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty()) lines.push_back(line);
    }
    return lines;
}

// A file "reads clean" if it exists and every non-empty line parses as
// JSON with the fields we expect. We don't verify hashes here — that
// happens in loadAll — this is just enough to decide primary vs backup.
bool readsClean(const std::string& path, std::vector<std::string>& outLines) {
    std::ifstream in(path, std::ios::binary);
    if (!in.good()) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    auto lines = splitLines(ss.str());
    for (const auto& l : lines) {
        try {
            json j = json::parse(l);
            if (!j.contains("algo") || !j.contains("iv_ct") || !j.contains("chain_hash")) return false;
        } catch (...) {
            return false;
        }
    }
    outLines = std::move(lines);
    return true;
}

std::string timestampNow() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    gmtime_r(&t, &tm);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y%m%dT%H%M%SZ");
    return oss.str();
}

void appendLine(const std::string& path, const std::string& line) {
    std::ofstream out(path, std::ios::app | std::ios::binary);
    out << line << "\n";
    out.flush();
}

} // namespace

PatientStorage::PatientStorage(std::string primaryPath, std::string backupPath, std::string passphrase)
    : primaryPath_(std::move(primaryPath)),
      backupPath_(std::move(backupPath)),
      passphrase_(std::move(passphrase)) {}

std::vector<std::string> PatientStorage::readUsableLines(bool& usedBackup) const {
    usedBackup = false;
    std::vector<std::string> primaryLines;
    std::vector<std::string> backupLines;
    bool primaryOk = readsClean(primaryPath_, primaryLines);
    bool backupOk = readsClean(backupPath_, backupLines);

    if (!primaryOk && !backupOk) return {}; // neither file usable; treated as an empty store
    if (!primaryOk) { usedBackup = true; return backupLines; }
    if (!backupOk) return primaryLines;

    // Both files parse cleanly. Under normal operation the backup is
    // written immediately after the primary on every append, so it can
    // only ever be zero or one record behind — never ahead, and never
    // more than one behind. If the primary has *fewer* records than the
    // backup, that's not a legitimate "fresh empty store" or ordinary lag:
    // it means the primary lost data (truncation, crash mid-write, an
    // accidental edit) while the backup still has the fuller history, so
    // the backup is the one to trust.
    if (primaryLines.size() < backupLines.size()) {
        usedBackup = true;
        return backupLines;
    }
    return primaryLines;
}

std::string PatientStorage::currentChainTip() const {
    bool usedBackup = false;
    auto lines = readUsableLines(usedBackup);
    if (lines.empty()) return "";
    json j = json::parse(lines.back());
    return j.value("chain_hash", "");
}

void PatientStorage::appendPatient(const Patient& patient, const CipherStrategy& cipher) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string plaintext = patient.toJson().dump();
    std::string ivAndCiphertext = cipher.encrypt(plaintext, passphrase_);
    std::string ivCtB64 = base64::encode(ivAndCiphertext);

    std::string prevHash = currentChainTip();
    std::string hash = chainHash(prevHash, ivCtB64);

    json record{
        {"algo", cipher.name()},
        {"iv_ct", ivCtB64},
        {"chain_hash", hash}
    };
    std::string line = record.dump();

    // Write to primary, then mirror to backup. If the process dies between
    // these two lines, readUsableLines() will notice the backup is now one
    // record behind but still internally consistent, and either file
    // remains independently valid — we never leave a half-written record.
    appendLine(primaryPath_, line);
    appendLine(backupPath_, line);
}

LoadResult PatientStorage::loadAll(const std::string& passphrase) const {
    std::lock_guard<std::mutex> lock(mutex_);

    LoadResult result;
    auto lines = readUsableLines(result.usedBackup);

    std::string prevHash;
    for (size_t i = 0; i < lines.size(); ++i) {
        LoadedRecord rec;
        try {
            json j = json::parse(lines[i]);
            std::string algo = j.value("algo", "");
            std::string ivCtB64 = j.value("iv_ct", "");
            std::string storedHash = j.value("chain_hash", "");
            rec.algo = algo;

            std::string expectedHash = chainHash(prevHash, ivCtB64);
            if (expectedHash != storedHash) {
                result.chainIntact = false;
                rec.ok = false;
                rec.problem = "hash chain mismatch at record " + std::to_string(i) +
                               " (record edited, reordered, or deleted)";
            }

            // Keep walking the chain from the hash actually stored on disk
            // (not the recomputed one) so a single tampered record shows up
            // as exactly one mismatch, and everything after it cascades —
            // that cascade is what makes tampering's location visible. This
            // must happen unconditionally, before we even attempt to
            // decrypt: if decryption below throws, the chain still needs to
            // advance, or every record *after* the bad one would falsely
            // inherit its "broken" status too.
            prevHash = storedHash;

            // Decryption failure is reported separately from a hash
            // mismatch — either can happen without the other (e.g. a
            // corrupted chain_hash field alone still decrypts fine).
            try {
                auto cipher = cipherFor(algo);
                std::string ivAndCiphertext = base64::decode(ivCtB64);
                std::string plaintext = cipher->decrypt(ivAndCiphertext, passphrase);
                rec.patient = Patient::fromJson(json::parse(plaintext));
            } catch (const std::exception& e) {
                rec.ok = false;
                std::string decryptProblem = std::string("failed to decrypt/parse record: ") + e.what();
                rec.problem = rec.problem.empty() ? decryptProblem : (rec.problem + "; " + decryptProblem);
            }
        } catch (const std::exception& e) {
            // The line itself wasn't valid JSON / didn't have the expected
            // fields. We can't know its true chain_hash, so the chain tip
            // can't be advanced reliably past this point.
            rec.ok = false;
            rec.problem = std::string("malformed record line: ") + e.what();
        }
        result.records.push_back(std::move(rec));
    }
    return result;
}

size_t PatientStorage::recordCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    bool usedBackup = false;
    return readUsableLines(usedBackup).size();
}

std::string PatientStorage::snapshotBackup() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string snapshotPath = primaryPath_ + "." + timestampNow() + ".snapshot";
    if (fs::exists(primaryPath_)) {
        fs::copy_file(primaryPath_, snapshotPath, fs::copy_options::overwrite_existing);
    } else {
        std::ofstream(snapshotPath, std::ios::binary); // create empty snapshot if store is empty
    }
    return snapshotPath;
}
```

> **A real bug this project hit, and why the fix looks the way it does:**
> the very first version updated `prevHash` *after* the decrypt call
> inside the same `try` block. When a tampered record failed to decrypt,
> the exception skipped that update — so the chain tip silently fell one
> record behind, and every *good* record after the tampered one was then
> falsely reported as broken too. The fix is exactly the line order shown
> above: advance `prevHash` unconditionally, before attempting decryption
> at all. Caught by actually corrupting a test file and checking the
> output, not by reading the code twice.

### Step 10 — The authentication layer: `Auth.h` / `Auth.cpp`

This is the piece that didn't exist in the first version of this project —
added specifically to close the "anyone who can reach the port can read
patient data" gap.

**Why passwords are hashed, not encrypted:** patient data (Step 7) is
*encrypted* because the server needs to get the original back. A password
is different — the server never needs to see it again, only needs to
answer "does this attempt match?" — so it's stored as a **hash**: a
one-way function where the same input always gives the same output, but
there's no way to run it backwards.

**Why plain SHA-256 isn't enough:** SHA-256 is *fast*, which is exactly
wrong for passwords — a stolen hash file lets an attacker try billions of
guesses per second on a GPU. **PBKDF2** fixes this by repeating the
underlying hash **200,000 times in a row** per attempt, on purpose. That's
unnoticeable for one real login and brutal for an attacker checking
millions of guesses. Each user also gets a random 16-byte **salt**, mixed
into every hash, so identical passwords don't produce identical stored
hashes and precomputed cracking tables don't work across users.

After a correct login, the server hands back a 256-bit random **session
token** instead of asking the browser to keep re-sending the actual
password — like a wristband stamped once at the door, then just shown
(not re-issued) on every re-entry. Tokens live in memory only, expire
after 2 hours, and vanish immediately on logout or server restart.

Create `backend/include/Auth.h`:

```cpp
#pragma once
#include <string>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <optional>

// Handles user accounts: storing them (hashed, never in plaintext) and
// checking a login attempt against what's stored.
//
// Passwords are hashed with PBKDF2-HMAC-SHA256 (see Auth.cpp for why this,
// rather than encrypting them like patient data). Each user gets their own
// random salt so two identical passwords never produce the same stored
// hash, and cracking one user's password doesn't help crack another's.
class UserStore {
public:
    explicit UserStore(std::string path);

    // Creates the very first admin account if the user file doesn't exist
    // yet. Returns true if it just created one (so main() can print the
    // generated password once).
    bool ensureDefaultAdmin(const std::string& username, const std::string& password);

    // Checks a login attempt. Returns true only if the username exists and
    // the password's hash matches what's on file.
    bool verifyCredentials(const std::string& username, const std::string& password) const;

private:
    std::string path_;
    mutable std::mutex mutex_;

    bool userExists(const std::string& username) const;
    void appendUser(const std::string& username, const std::string& password);
};

// Issues and checks short-lived session tokens after a successful login.
// Tokens live only in memory (a std::unordered_map) — restarting the
// server logs everyone out, which is the right trade-off for a small
// prototype (no session database to keep in sync or clean up).
class SessionManager {
public:
    explicit SessionManager(std::chrono::minutes ttl = std::chrono::minutes(120));

    // Generates a new random token for `username` and remembers it.
    std::string createSession(const std::string& username);

    // Returns the username for a still-valid token, or std::nullopt if the
    // token is unknown or has expired.
    std::optional<std::string> validate(const std::string& token) const;

    // Forgets a token immediately (used by /api/logout).
    void invalidate(const std::string& token);

private:
    struct Session {
        std::string username;
        std::chrono::steady_clock::time_point expiresAt;
    };
    std::chrono::minutes ttl_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, Session> sessions_;
};

// Pulls the token out of a "Bearer <token>" Authorization header value.
// Returns "" if the header is missing or malformed.
std::string extractBearerToken(const std::string& authorizationHeader);
```

Create `backend/src/Auth.cpp`:

```cpp
#include "Auth.h"
#include "Base64.h"
#include "third_party/json.hpp"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iomanip>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {

constexpr int kSaltLen = 16;
constexpr int kHashLen = 32;      // SHA-256 output size
constexpr int kIterations = 200000; // deliberately slow — see the explanation above

std::string randomBytes(int len) {
    std::string buf(len, '\0');
    if (RAND_bytes(reinterpret_cast<unsigned char*>(buf.data()), len) != 1) {
        throw std::runtime_error("RAND_bytes failed");
    }
    return buf;
}

// PBKDF2-HMAC-SHA256: hashes `password` together with `salt`, and does it
// kIterations times in a row on purpose. That deliberate slowness is the
// entire point for passwords (unlike AES/DES, which are built to be fast).
std::string pbkdf2(const std::string& password, const std::string& salt) {
    std::string out(kHashLen, '\0');
    int ok = PKCS5_PBKDF2_HMAC(password.data(), static_cast<int>(password.size()),
                                reinterpret_cast<const unsigned char*>(salt.data()), static_cast<int>(salt.size()),
                                kIterations, EVP_sha256(),
                                kHashLen, reinterpret_cast<unsigned char*>(out.data()));
    if (!ok) throw std::runtime_error("PKCS5_PBKDF2_HMAC failed");
    return out;
}

// Constant-time comparison: a naive `a == b` returns as soon as it finds a
// mismatching byte, so how quickly it returns leaks how many leading bytes
// were correct (a "timing side channel"). This walks every byte regardless,
// so it always takes the same time whether the guess is close or nowhere
// close, which prevents that leak.
bool constantTimeEquals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (size_t i = 0; i < a.size(); ++i) diff |= (unsigned char)a[i] ^ (unsigned char)b[i];
    return diff == 0;
}

std::string toHex(const std::string& raw) {
    std::ostringstream oss;
    for (unsigned char c : raw) oss << std::hex << std::setw(2) << std::setfill('0') << (int)c;
    return oss.str();
}

} // namespace

UserStore::UserStore(std::string path) : path_(std::move(path)) {}

bool UserStore::userExists(const std::string& username) const {
    std::ifstream in(path_, std::ios::binary);
    if (!in.good()) return false;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        try {
            json j = json::parse(line);
            if (j.value("username", "") == username) return true;
        } catch (...) { /* skip malformed line */ }
    }
    return false;
}

void UserStore::appendUser(const std::string& username, const std::string& password) {
    std::string salt = randomBytes(kSaltLen);
    std::string hash = pbkdf2(password, salt);

    json record{
        {"username", username},
        {"salt", base64::encode(salt)},
        {"hash", base64::encode(hash)},
        {"iterations", kIterations}
    };

    fs::create_directories(fs::path(path_).parent_path());
    std::ofstream out(path_, std::ios::app | std::ios::binary);
    out << record.dump() << "\n";
}

bool UserStore::ensureDefaultAdmin(const std::string& username, const std::string& password) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (fs::exists(path_)) return false; // already initialized, don't overwrite
    appendUser(username, password);
    return true;
}

bool UserStore::verifyCredentials(const std::string& username, const std::string& password) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ifstream in(path_, std::ios::binary);
    if (!in.good()) return false;

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        try {
            json j = json::parse(line);
            if (j.value("username", "") != username) continue;

            std::string salt = base64::decode(j.value("salt", ""));
            std::string storedHash = base64::decode(j.value("hash", ""));
            std::string attemptHash = pbkdf2(password, salt);
            return constantTimeEquals(attemptHash, storedHash);
        } catch (...) {
            continue; // malformed line; keep looking
        }
    }
    return false; // username not found
}

// ---------------------------------------------------------------------

SessionManager::SessionManager(std::chrono::minutes ttl) : ttl_(ttl) {}

std::string SessionManager::createSession(const std::string& username) {
    std::string token = toHex(randomBytes(32)); // 256 bits of randomness, printed as hex
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_[token] = Session{username, std::chrono::steady_clock::now() + ttl_};
    return token;
}

std::optional<std::string> SessionManager::validate(const std::string& token) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(token);
    if (it == sessions_.end()) return std::nullopt;
    if (std::chrono::steady_clock::now() > it->second.expiresAt) return std::nullopt;
    return it->second.username;
}

void SessionManager::invalidate(const std::string& token) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(token);
}

std::string extractBearerToken(const std::string& authorizationHeader) {
    const std::string prefix = "Bearer ";
    if (authorizationHeader.rfind(prefix, 0) != 0) return "";
    return authorizationHeader.substr(prefix.size());
}
```

### Step 11 — Wire it all together: `main.cpp`

This is deliberately the thinnest file in the project: parse the request,
call one method on `storage`/`userStore`/`sessions`, serialize the result.
If a route ever needs real logic beyond that, it belongs on one of the
classes above instead — `main.cpp` should stay a pure wiring layer.

Create `backend/src/main.cpp`:

```cpp
// Patient Records REST API
//
// A small C++ HTTP server (cpp-httplib) fronting an encrypted, append-only,
// tamper-evident file store (PatientStorage). Two interchangeable ciphers
// (AesCipher, DesCipher — see Cipher.h) are selectable per record via the
// Strategy pattern. This file just wires HTTP requests to those classes;
// it holds no business logic of its own.
#include "third_party/httplib.h"
#include "third_party/json.hpp"
#include "Patient.h"
#include "Cipher.h"
#include "Storage.h"
#include "Auth.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstdlib>
#include <filesystem>
#include <openssl/rand.h>

using json = nlohmann::json;

namespace {

std::string nextPatientId(size_t currentCount) {
    std::ostringstream oss;
    oss << "P-" << std::setw(6) << std::setfill('0') << (currentCount + 1);
    return oss.str();
}

// Maps the short algorithm codes the frontend sends ("AES"/"DES") to the
// full names CipherStrategy::name() actually persists on disk.
std::unique_ptr<CipherStrategy> cipherFromRequestCode(const std::string& code) {
    if (code == "DES") return std::make_unique<DesCipher>();
    return std::make_unique<AesCipher>(); // default / "AES"
}

json recordToJson(const LoadedRecord& r) {
    json j = r.ok ? r.patient.toJson() : json::object();
    j["algo"] = r.algo;
    j["ok"] = r.ok;
    if (!r.ok) j["problem"] = r.problem;
    return j;
}

// A short, readable random password for the auto-created admin account
// (used only when ADMIN_PASSWORD isn't set). Built from random bytes
// mapped into a fixed alphabet rather than base64, so it never contains
// characters that are awkward to copy/paste or type (+, /, =).
std::string randomReadablePassword(int length = 14) {
    static const char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnpqrstuvwxyz23456789";
    std::string buf(length, '\0');
    if (RAND_bytes(reinterpret_cast<unsigned char*>(buf.data()), length) != 1) {
        throw std::runtime_error("RAND_bytes failed");
    }
    std::string out;
    out.reserve(length);
    for (unsigned char c : buf) out += alphabet[c % (sizeof(alphabet) - 1)];
    return out;
}

} // namespace

int main() {
    // The passphrase that derives the AES/DES keys. In a real deployment
    // this MUST come from a secret manager / environment, never a
    // hardcoded default — we fall back to a demo value only so the server
    // is runnable out of the box, and we shout about it loudly.
    const char* envKey = std::getenv("PATIENT_DB_KEY");
    std::string passphrase = envKey ? std::string(envKey) : "demo-only-insecure-passphrase";
    if (!envKey) {
        std::cerr << "[WARN] PATIENT_DB_KEY is not set. Using a demo passphrase.\n"
                     "       Set PATIENT_DB_KEY before running with real data.\n";
    }

    const std::string dataDir = "./data";
    std::filesystem::create_directories(dataDir);
    PatientStorage storage(dataDir + "/patients.dat", dataDir + "/patients.dat.bak", passphrase);

    // --- Auth setup ---------------------------------------------------
    UserStore userStore(dataDir + "/users.dat");
    SessionManager sessions; // 2-hour tokens, see Auth.h default

    std::string adminUser = std::getenv("ADMIN_USERNAME") ? std::getenv("ADMIN_USERNAME") : "admin";
    const char* envAdminPass = std::getenv("ADMIN_PASSWORD");
    std::string adminPass = envAdminPass ? std::string(envAdminPass) : randomReadablePassword();

    bool justCreated = userStore.ensureDefaultAdmin(adminUser, adminPass);
    if (justCreated) {
        std::cout << "\n"
                     "======================================================\n"
                     " Created initial admin account (data/users.dat):\n"
                     "   username: " << adminUser << "\n"
                     "   password: " << (envAdminPass ? "(the one you set in ADMIN_PASSWORD)" : adminPass) << "\n"
                     " This is printed ONCE. Save it now.\n"
                     "======================================================\n\n";
        std::cout.flush(); // make sure this reaches the terminal/log immediately, not just on exit
    }

    // Checks the Authorization header on a protected request. On failure it
    // writes the 401 response itself and returns false, so a handler can
    // just `if (!requireAuth(req, res)) return;` as its first line.
    auto requireAuth = [&](const httplib::Request& req, httplib::Response& res) -> bool {
        std::string token = extractBearerToken(req.get_header_value("Authorization"));
        if (!token.empty() && sessions.validate(token).has_value()) return true;
        res.status = 401;
        res.set_content(json{{"error", "login required"}}.dump(), "application/json");
        return false;
    };

    httplib::Server svr;

    // Serve the dashboard frontend directly so the browser and API share
    // an origin (no CORS setup needed for local/demo use). Note the HTML/JS
    // itself is public (the browser has to load the login page from
    // somewhere) — what's actually protected is the /api/patients data
    // underneath it, via requireAuth above.
    auto frontendDir = std::getenv("FRONTEND_DIR") ? std::string(std::getenv("FRONTEND_DIR")) : "../frontend";
    svr.set_mount_point("/", frontendDir);

    svr.Post("/api/login", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            json body = json::parse(req.body);
            std::string username = body.value("username", "");
            std::string password = body.value("password", "");
            if (userStore.verifyCredentials(username, password)) {
                std::string token = sessions.createSession(username);
                res.set_content(json{{"token", token}, {"username", username}}.dump(), "application/json");
            } else {
                res.status = 401;
                res.set_content(json{{"error", "invalid username or password"}}.dump(), "application/json");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    svr.Post("/api/logout", [&](const httplib::Request& req, httplib::Response& res) {
        sessions.invalidate(extractBearerToken(req.get_header_value("Authorization")));
        res.set_content(json{{"ok", true}}.dump(), "application/json");
    });

    svr.Get("/api/patients", [&](const httplib::Request& req, httplib::Response& res) {
        if (!requireAuth(req, res)) return;
        LoadResult result = storage.loadAll(passphrase);
        json out;
        out["chainIntact"] = result.chainIntact;
        out["usedBackup"] = result.usedBackup;
        out["count"] = result.records.size();
        json arr = json::array();
        for (const auto& r : result.records) arr.push_back(recordToJson(r));
        out["patients"] = arr;
        res.set_content(out.dump(), "application/json");
    });

    svr.Post("/api/patients", [&](const httplib::Request& req, httplib::Response& res) {
        if (!requireAuth(req, res)) return;
        try {
            json body = json::parse(req.body);
            Patient p;
            p.id = nextPatientId(storage.recordCount());
            p.name = body.value("name", "");
            p.age = body.value("age", 0);
            p.gender = body.value("gender", "");
            p.contact = body.value("contact", "");
            p.diagnosis = body.value("diagnosis", "");
            p.admissionDate = body.value("admissionDate", "");

            if (p.name.empty()) {
                res.status = 400;
                res.set_content(json{{"error", "name is required"}}.dump(), "application/json");
                return;
            }

            std::string algoCode = body.value("algo", "AES");
            auto cipher = cipherFromRequestCode(algoCode);
            storage.appendPatient(p, *cipher);

            json out = p.toJson();
            out["algo"] = cipher->name();
            res.status = 201;
            res.set_content(out.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    svr.Post("/api/backup", [&](const httplib::Request& req, httplib::Response& res) {
        if (!requireAuth(req, res)) return;
        std::string path = storage.snapshotBackup();
        res.set_content(json{{"snapshot", path}}.dump(), "application/json");
    });

    svr.Get("/api/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(json{{"status", "ok"}}.dump(), "application/json");
    });

    std::cout << "Patient dashboard API listening on http://localhost:8080" << std::endl;
    svr.listen("0.0.0.0", 8080);
    return 0;
}
```

### Step 12 — Tell CMake about all of it

Create `backend/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.10)
project(patient_dashboard_backend CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

add_executable(patient_server
    src/main.cpp
    src/Cipher.cpp
    src/Storage.cpp
    src/Auth.cpp
)

target_include_directories(patient_server PRIVATE include)
target_link_libraries(patient_server PRIVATE OpenSSL::SSL OpenSSL::Crypto Threads::Threads)
```

### Step 13 — The frontend dashboard

One file: a login screen, an "add patient" form, and a table of records.
Every data-bearing call goes through a single wrapper (`apiFetch`) that
attaches the session token to every request and treats an HTTP 401 as
"show the login screen again" — so no other code in the file has to think
about authentication at all.

Create `frontend/index.html`:

```html
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Patient Records Dashboard</title>
<style>
  :root {
    --bg: #f4f6f8;
    --card: #ffffff;
    --border: #dfe3e8;
    --text: #1f2933;
    --muted: #6b7785;
    --accent: #2563eb;
    --ok: #16a34a;
    --ok-bg: #ecfdf3;
    --bad: #dc2626;
    --bad-bg: #fef2f2;
    --warn: #b45309;
    --warn-bg: #fffbeb;
  }
  * { box-sizing: border-box; }
  body {
    margin: 0;
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
    background: var(--bg);
    color: var(--text);
  }
  header {
    padding: 20px 28px;
    background: var(--card);
    border-bottom: 1px solid var(--border);
    display: flex;
    align-items: baseline;
    justify-content: space-between;
    flex-wrap: wrap;
    gap: 8px;
  }
  header h1 { font-size: 18px; margin: 0; }
  header p { margin: 0; color: var(--muted); font-size: 13px; }
  main {
    max-width: 1080px;
    margin: 24px auto;
    padding: 0 20px 40px;
    display: grid;
    gap: 20px;
  }
  .card {
    background: var(--card);
    border: 1px solid var(--border);
    border-radius: 10px;
    padding: 20px;
  }
  .status-row {
    display: flex;
    gap: 12px;
    flex-wrap: wrap;
    align-items: center;
  }
  .pill {
    padding: 6px 12px;
    border-radius: 999px;
    font-size: 13px;
    font-weight: 600;
  }
  .pill.ok { background: var(--ok-bg); color: var(--ok); }
  .pill.bad { background: var(--bad-bg); color: var(--bad); }
  .pill.warn { background: var(--warn-bg); color: var(--warn); }
  button {
    font: inherit;
    border: 1px solid var(--border);
    background: var(--card);
    padding: 8px 14px;
    border-radius: 8px;
    cursor: pointer;
    color: var(--text);
  }
  button.primary {
    background: var(--accent);
    border-color: var(--accent);
    color: white;
    font-weight: 600;
  }
  button:hover { filter: brightness(0.97); }
  form {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
    gap: 14px;
  }
  label {
    display: flex;
    flex-direction: column;
    gap: 4px;
    font-size: 13px;
    color: var(--muted);
  }
  input, select {
    font: inherit;
    padding: 8px 10px;
    border: 1px solid var(--border);
    border-radius: 6px;
    background: white;
    color: var(--text);
  }
  form .full { grid-column: 1 / -1; }
  .algo-hint { font-size: 12px; color: var(--warn); margin-top: 4px; display: none; }
  select[data-algo="DES"] ~ .algo-hint,
  #algoHintDes.show { display: block; }
  table { width: 100%; border-collapse: collapse; font-size: 14px; }
  th, td { text-align: left; padding: 10px 8px; border-bottom: 1px solid var(--border); }
  th { color: var(--muted); font-weight: 600; font-size: 12px; text-transform: uppercase; letter-spacing: 0.03em; }
  tr.corrupted td { background: var(--bad-bg); }
  .badge { font-size: 11px; padding: 2px 8px; border-radius: 999px; background: #eef2ff; color: var(--accent); font-weight: 600; }
  .badge.des { background: var(--warn-bg); color: var(--warn); }
  .empty { color: var(--muted); padding: 20px 0; text-align: center; }
  .toolbar { display: flex; justify-content: space-between; align-items: center; margin-bottom: 8px; flex-wrap: wrap; gap: 10px; }
  .muted { color: var(--muted); font-size: 13px; }

  #loginScreen {
    min-height: 100vh;
    display: flex;
    align-items: center;
    justify-content: center;
  }
  #loginScreen .card { width: 100%; max-width: 340px; }
  #loginScreen h1 { font-size: 17px; margin: 0 0 4px; }
  #loginScreen p.sub { margin: 0 0 18px; color: var(--muted); font-size: 13px; }
  #loginScreen form { grid-template-columns: 1fr; }
  #loginError { color: var(--bad); font-size: 13px; min-height: 18px; margin-top: 4px; }
</style>
</head>
<body>

<section id="loginScreen">
  <div class="card">
    <h1>Patient Records Dashboard</h1>
    <p class="sub">Sign in to view or add patient records.</p>
    <form id="loginForm">
      <label>Username
        <input type="text" name="username" autocomplete="username" required>
      </label>
      <label>Password
        <input type="password" name="password" autocomplete="current-password" required>
      </label>
      <button type="submit" class="primary">Sign in</button>
      <div id="loginError"></div>
    </form>
  </div>
</section>

<div id="appScreen" hidden>

<header>
  <div>
    <h1>Patient Records Dashboard</h1>
    <p>Backed by a C++ REST API &middot; records encrypted at rest (AES-256 / DES) &middot; append-only, hash-chained storage</p>
  </div>
  <div class="status-row" id="statusRow">
    <span class="muted" id="whoAmI"></span>
    <button id="logoutBtn">Log out</button>
  </div>
</header>

<main>
  <section class="card">
    <h2 style="margin-top:0;font-size:15px;">Add Patient</h2>
    <form id="patientForm">
      <label class="full">Full name
        <input type="text" name="name" required>
      </label>
      <label>Age
        <input type="number" name="age" min="0" max="130">
      </label>
      <label>Gender
        <input type="text" name="gender" placeholder="e.g. Female">
      </label>
      <label>Contact
        <input type="text" name="contact" placeholder="phone / email">
      </label>
      <label>Admission date
        <input type="date" name="admissionDate">
      </label>
      <label class="full">Diagnosis / notes
        <input type="text" name="diagnosis">
      </label>
      <label>
        Encryption
        <select name="algo" id="algoSelect">
          <option value="AES" selected>AES-256 (recommended)</option>
          <option value="DES">DES (legacy, insecure — demo only)</option>
        </select>
        <span class="algo-hint" id="algoHintDes">DES's 56-bit key can be brute-forced in hours. Only use it here to compare against AES.</span>
      </label>
      <div class="full" style="display:flex; gap:10px; align-items:end;">
        <button type="submit" class="primary">Add patient</button>
      </div>
    </form>
  </section>

  <section class="card">
    <div class="toolbar">
      <h2 style="margin:0;font-size:15px;">Records</h2>
      <div style="display:flex; gap:10px; align-items:center;">
        <span class="muted" id="recordCount"></span>
        <button id="backupBtn">Snapshot backup</button>
        <button id="refreshBtn">Refresh</button>
      </div>
    </div>
    <div id="tableWrap"></div>
  </section>
</main>

</div><!-- #appScreen -->

<script>
const API = "";

// The session token lives in sessionStorage: it survives a page refresh
// (so reloading the tab doesn't force a re-login) but disappears when the
// tab is closed, and is never sent anywhere except in our own API calls.
function getToken() { return sessionStorage.getItem("token"); }
function setToken(t) { sessionStorage.setItem("token", t); }
function clearToken() { sessionStorage.removeItem("token"); }

// Wraps fetch() to attach "Authorization: Bearer <token>" automatically,
// and treats a 401 response as "you're not logged in (any more)" — the
// token may have expired or the server may have restarted.
async function apiFetch(path, options = {}) {
  const headers = Object.assign({}, options.headers, {
    "Authorization": "Bearer " + (getToken() || "")
  });
  const res = await fetch(API + path, { ...options, headers });
  if (res.status === 401) {
    clearToken();
    showLogin();
    throw new Error("session expired, please sign in again");
  }
  return res;
}

function showLogin() {
  document.getElementById("loginScreen").hidden = false;
  document.getElementById("appScreen").hidden = true;
}

function showApp(username) {
  document.getElementById("loginScreen").hidden = true;
  document.getElementById("appScreen").hidden = false;
  document.getElementById("whoAmI").textContent = username ? ("Signed in as " + username) : "";
  fetchPatients().catch(err => console.error(err));
}

document.getElementById("loginForm").addEventListener("submit", async (e) => {
  e.preventDefault();
  const form = e.target;
  const errorBox = document.getElementById("loginError");
  errorBox.textContent = "";
  try {
    const res = await fetch(API + "/api/login", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        username: form.username.value.trim(),
        password: form.password.value
      })
    });
    const data = await res.json();
    if (!res.ok) {
      errorBox.textContent = data.error || "Sign in failed";
      return;
    }
    setToken(data.token);
    form.reset();
    showApp(data.username);
  } catch (err) {
    errorBox.textContent = "Could not reach the server.";
  }
});

document.getElementById("logoutBtn").addEventListener("click", async () => {
  try { await apiFetch("/api/logout", { method: "POST" }); } catch (_) { /* ignore */ }
  clearToken();
  showLogin();
});

async function fetchPatients() {
  const res = await apiFetch("/api/patients");
  const data = await res.json();
  renderStatus(data);
  renderTable(data);
}

function renderStatus(data) {
  const row = document.getElementById("statusRow");
  row.innerHTML = "";
  const chain = document.createElement("span");
  chain.className = "pill " + (data.chainIntact ? "ok" : "bad");
  chain.textContent = data.chainIntact ? "Integrity chain OK" : "Integrity chain BROKEN";
  row.appendChild(chain);

  if (data.usedBackup) {
    const warn = document.createElement("span");
    warn.className = "pill warn";
    warn.textContent = "Recovered from backup file";
    row.appendChild(warn);
  }
}

function renderTable(data) {
  document.getElementById("recordCount").textContent = data.count + " record(s)";
  const wrap = document.getElementById("tableWrap");
  if (!data.patients.length) {
    wrap.innerHTML = '<div class="empty">No patients yet. Add the first one above.</div>';
    return;
  }
  const rows = data.patients.map(p => {
    if (!p.ok) {
      return `<tr class="corrupted">
        <td colspan="7">&#9888; ${escapeHtml(p.problem || "record failed verification")}</td>
      </tr>`;
    }
    const badgeClass = p.algo && p.algo.startsWith("DES") ? "badge des" : "badge";
    return `<tr>
      <td>${escapeHtml(p.id)}</td>
      <td>${escapeHtml(p.name)}</td>
      <td>${escapeHtml(String(p.age ?? ""))}</td>
      <td>${escapeHtml(p.gender || "")}</td>
      <td>${escapeHtml(p.contact || "")}</td>
      <td>${escapeHtml(p.diagnosis || "")}</td>
      <td>${escapeHtml(p.admissionDate || "")}</td>
      <td><span class="${badgeClass}">${escapeHtml(p.algo || "")}</span></td>
    </tr>`;
  }).join("");

  wrap.innerHTML = `<table>
    <thead><tr>
      <th>ID</th><th>Name</th><th>Age</th><th>Gender</th><th>Contact</th><th>Diagnosis</th><th>Admitted</th><th>Cipher</th>
    </tr></thead>
    <tbody>${rows}</tbody>
  </table>`;
}

function escapeHtml(s) {
  return String(s).replace(/[&<>"']/g, c => ({
    "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;"
  }[c]));
}

document.getElementById("patientForm").addEventListener("submit", async (e) => {
  e.preventDefault();
  const form = e.target;
  const payload = {
    name: form.name.value.trim(),
    age: form.age.value ? Number(form.age.value) : 0,
    gender: form.gender.value.trim(),
    contact: form.contact.value.trim(),
    admissionDate: form.admissionDate.value,
    diagnosis: form.diagnosis.value.trim(),
    algo: form.algo.value
  };
  const res = await apiFetch("/api/patients", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(payload)
  });
  if (!res.ok) {
    const err = await res.json().catch(() => ({}));
    alert("Could not add patient: " + (err.error || res.statusText));
    return;
  }
  form.reset();
  await fetchPatients();
});

document.getElementById("refreshBtn").addEventListener("click", fetchPatients);

document.getElementById("backupBtn").addEventListener("click", async () => {
  const res = await apiFetch("/api/backup", { method: "POST" });
  const data = await res.json();
  alert("Snapshot saved: " + data.snapshot);
});

document.getElementById("algoSelect").addEventListener("change", (e) => {
  document.getElementById("algoHintDes").classList.toggle("show", e.target.value === "DES");
});

// On load: if we already have a token in this tab (e.g. after a refresh),
// go straight to the dashboard and let apiFetch's 401 handling bounce us
// back to the login screen if that token turns out to be stale.
if (getToken()) {
  showApp("");
} else {
  showLogin();
}
</script>

</body>
</html>
```

---

## Part 3 — Build and run

### Step 14 — Compile

```bash
cd backend
mkdir -p build && cd build
cmake ..
make
cd ..
```

You should see `[100%] Built target patient_server` with no errors or
warnings. The binary is at `backend/build/patient_server`.

### Step 15 — Choose your environment variables

| Variable | Required? | Purpose |
|---|---|---|
| `PATIENT_DB_KEY` | strongly recommended | Passphrase that derives the AES/DES encryption key. Without it, the server runs anyway (for convenience) using a hardcoded demo passphrase and prints a warning — never do that with real data. |
| `ADMIN_USERNAME` | optional (defaults to `admin`) | Username for the auto-created first login |
| `ADMIN_PASSWORD` | optional | Password for the auto-created first login. If you don't set it, a random 14-character password is generated and printed **once**, at first startup only — save it before it scrolls away. |
| `FRONTEND_DIR` | optional (defaults to `../frontend`) | Where to serve the dashboard's static files from |

### Step 16 — Run it

**Run this from the `backend/` folder, not `backend/build/`** — the server
serves the frontend from a path relative to its own current directory
(`../frontend` by default), so launching it from the wrong folder silently
breaks that. This was an actual mistake made and caught while building
this project, not a hypothetical warning.

```bash
ADMIN_USERNAME="doctor" ADMIN_PASSWORD="choose-a-real-password" \
PATIENT_DB_KEY="choose-a-real-passphrase" \
./build/patient_server
```

You should see:

```
======================================================
 Created initial admin account (data/users.dat):
   username: doctor
   password: (the one you set in ADMIN_PASSWORD)
 This is printed ONCE. Save it now.
======================================================

Patient dashboard API listening on http://localhost:8080
```

This creates a `data/` folder next to wherever you ran the command from,
containing `patients.dat`, `patients.dat.bak`, and `users.dat`.

---

## Part 4 — Use the dashboard

### Step 17 — Log in

Open **http://localhost:8080** in a browser. You'll see a login screen —
sign in with the admin username/password from Step 16.

### Step 18 — Add a patient

Fill in the "Add Patient" form (name is the only required field), pick
AES or DES from the "Encryption" dropdown, and submit. The record appears
in the table below with its assigned ID (`P-000001`, `P-000002`, ...) and
which cipher protected it.

### Step 19 — Watch the integrity and backup indicators

The pills at the top of the dashboard show **"Integrity chain OK"** (the
hash chain from Step 9 checks out) and, if it ever had to recover from the
mirror file, **"Recovered from backup file."** The **"Snapshot backup"**
button makes an explicit, timestamped copy of the whole store on demand.

### Step 20 — Log out

The **"Log out"** button invalidates your session token immediately —
using it again afterward correctly gets rejected (verified in Step 21).

---

## Part 5 — Verify it actually works

Don't take any of the above on faith — run these yourself. Every one of
them was actually executed against a running instance of this exact code
before being written down here.

### Step 21 — Test the auth wall

```bash
# 1. no token → 401
curl -s -o /dev/null -w "%{http_code}\n" http://localhost:8080/api/patients

# 2. wrong password → 401
curl -s -X POST http://localhost:8080/api/login -H "Content-Type: application/json" \
  -d '{"username":"doctor","password":"wrong"}'

# 3. correct password → 200 + a token
TOKEN=$(curl -s -X POST http://localhost:8080/api/login -H "Content-Type: application/json" \
  -d '{"username":"doctor","password":"choose-a-real-password"}' \
  | python3 -c "import json,sys;print(json.load(sys.stdin)['token'])")

# 4. same request, now with the token → 200
curl -s http://localhost:8080/api/patients -H "Authorization: Bearer $TOKEN"

# 5. add a patient with the token → 201
curl -s -X POST http://localhost:8080/api/patients \
  -H "Authorization: Bearer $TOKEN" -H "Content-Type: application/json" \
  -d '{"name":"Test Patient","age":40,"algo":"AES"}'

# 6. log out, then reuse the same token → 401
curl -s -X POST http://localhost:8080/api/logout -H "Authorization: Bearer $TOKEN"
curl -s -o /dev/null -w "%{http_code}\n" http://localhost:8080/api/patients -H "Authorization: Bearer $TOKEN"
```

### Step 22 — Confirm patient data is actually encrypted on disk

```bash
# log in again first to get a fresh token, add a patient named e.g. "Asha Rao", then:
grep -i "Asha" data/patients.dat || echo "OK: no plaintext leak"
cat data/patients.dat   # you should see base64 ciphertext, never the name you typed
```

### Step 23 — Test tamper detection

```bash
# flip one character inside a stored record's iv_ct field
python3 - <<'EOF'
import json
lines = open("data/patients.dat").read().splitlines()
rec = json.loads(lines[0])
ct = list(rec["iv_ct"]); ct[len(ct)//2] = 'A' if ct[len(ct)//2] != 'A' else 'B'
rec["iv_ct"] = "".join(ct)
lines[0] = json.dumps(rec)
open("data/patients.dat", "w").write("\n".join(lines) + "\n")
EOF

curl -s http://localhost:8080/api/patients -H "Authorization: Bearer $TOKEN"
# expect: "chainIntact": false, and only that one record marked "ok": false
```

### Step 24 — Test backup recovery

```bash
: > data/patients.dat    # simulate a crash mid-write / disk corruption
curl -s http://localhost:8080/api/patients -H "Authorization: Bearer $TOKEN"
# expect: your patients are still there, and "usedBackup": true
```

### Step 25 — Shut it down cleanly

```bash
# find and stop the server process
pgrep -x patient_server
kill <pid-from-above>
```

---

## API reference

| Method | Path | Auth required | Body | Purpose |
|---|---|---|---|---|
| POST | `/api/login` | no | `{username, password}` | log in, returns `{token, username}` |
| POST | `/api/logout` | yes | — | invalidate the current token |
| GET | `/api/patients` | yes | — | list all patients, decrypted, with integrity status |
| POST | `/api/patients` | yes | `{name, age, gender, contact, diagnosis, admissionDate, algo: "AES"\|"DES"}` | add a patient |
| POST | `/api/backup` | yes | — | force a timestamped snapshot |
| GET | `/api/health` | no | — | liveness check |

"Auth required" means the request needs an `Authorization: Bearer <token>`
header from a successful `/api/login`; a missing or expired token gets a
401 response.

## Environment variables reference

| Variable | Default if unset | Effect |
|---|---|---|
| `PATIENT_DB_KEY` | `"demo-only-insecure-passphrase"` (with a console warning) | Passphrase deriving the AES/DES key |
| `ADMIN_USERNAME` | `admin` | Username for the first auto-created account |
| `ADMIN_PASSWORD` | a random 14-character password, printed once | Password for the first auto-created account |
| `FRONTEND_DIR` | `../frontend` | Path the server serves static dashboard files from |

## Known limitations / next steps

- Key derivation for AES/DES is SHA-256, not PBKDF2/Argon2 — fine for a
  prototype, not for production secrets handling. (Passwords, by
  contrast, do use PBKDF2 — see Step 10.)
- **No TLS/HTTPS.** The session token travels as a plain header; fine on
  `localhost`, not fine on a real network. Put a reverse proxy with a real
  certificate in front of this before exposing it beyond your own machine.
- No per-user roles (e.g. doctor vs admin) yet — every logged-in account
  has full access. `UserStore` already supports multiple accounts; only
  role-based permission checks are missing.
- No rate-limiting/lockout on repeated failed logins.
- IDs are assigned from the current record count, so they're stable only
  as long as no record is deleted (there's no delete endpoint yet, by
  design — this is an append-only ledger).
