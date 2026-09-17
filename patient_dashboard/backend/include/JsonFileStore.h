#pragma once
#include <string>
#include <mutex>
#include <fstream>
#include <sstream>
#include "third_party/json.hpp"

// Minimal thread-safe JSON-array file store: the whole collection lives as
// one JSON array on disk and is rewritten atomically-ish on every mutation
// (open+truncate+write; fine at this scale). Used for the simpler entities
// (users, doctor availability, appointments, documents, notifications) that
// don't need PatientStorage's tamper-evident hash chain -- that guarantee
// is specifically about medical records, not app bookkeeping data.
class JsonFileStore {
public:
    explicit JsonFileStore(std::string path) : path_(std::move(path)) {}

    nlohmann::json readAll() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return readAllLocked();
    }

    // Appends one item (caller should already have set its id) and persists.
    void append(const nlohmann::json& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        nlohmann::json arr = readAllLocked();
        arr.push_back(item);
        writeLocked(arr);
    }

    // Replaces the whole collection -- used for in-place updates like
    // marking a slot booked or a notification read.
    void writeAll(const nlohmann::json& arr) {
        std::lock_guard<std::mutex> lock(mutex_);
        writeLocked(arr);
    }

    size_t count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return readAllLocked().size();
    }

private:
    std::string path_;
    mutable std::mutex mutex_;

    nlohmann::json readAllLocked() const {
        std::ifstream in(path_, std::ios::binary);
        if (!in.good()) return nlohmann::json::array();
        std::ostringstream ss;
        ss << in.rdbuf();
        std::string content = ss.str();
        if (content.empty()) return nlohmann::json::array();
        try {
            nlohmann::json j = nlohmann::json::parse(content);
            if (!j.is_array()) return nlohmann::json::array();
            return j;
        } catch (...) {
            return nlohmann::json::array();
        }
    }

    void writeLocked(const nlohmann::json& arr) {
        std::ofstream out(path_, std::ios::binary | std::ios::trunc);
        out << arr.dump();
    }
};
