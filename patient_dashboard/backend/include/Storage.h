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
