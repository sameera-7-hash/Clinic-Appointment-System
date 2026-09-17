#include "Storage.h"
#include "Base64.h"
#include "PortableTime.h"
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
    std::tm tm = gmtimeUtc(t);
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
