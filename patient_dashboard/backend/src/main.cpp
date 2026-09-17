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
#include "Base64.h"
#include "JsonFileStore.h"
#include "PasswordHash.h"
#include "PortableTime.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstdlib>
#include <filesystem>
#include <ctime>

using json = nlohmann::json;

namespace {

std::string nextPatientId(size_t currentCount) {
    std::ostringstream oss;
    oss << "P-" << std::setw(6) << std::setfill('0') << (currentCount + 1);
    return oss.str();
}

// Same id-minting scheme as nextPatientId, generalized for the other
// entities added alongside patient records (users, appointments, documents,
// notifications) so each gets its own readable, sequential id space.
std::string nextId(const std::string& prefix, size_t currentCount) {
    std::ostringstream oss;
    oss << prefix << std::setw(6) << std::setfill('0') << (currentCount + 1);
    return oss.str();
}

std::string nowIso() {
    std::time_t t = std::time(nullptr);
    std::tm tm = gmtimeUtc(t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

// Strips the public fields out of a stored user record -- callers must
// never let the password salt/hash reach an HTTP response.
json publicUser(const json& u) {
    return json{
        {"id", u.value("id", "")},
        {"name", u.value("name", "")},
        {"email", u.value("email", "")},
        {"role", u.value("role", "")}
    };
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

    // Simpler app-bookkeeping entities (not medical records, so they don't
    // need PatientStorage's encryption/hash-chain machinery -- a plain
    // JSON-array file each is enough).
    JsonFileStore usersStore(dataDir + "/users.json");
    JsonFileStore availabilityStore(dataDir + "/availability.json");
    JsonFileStore appointmentsStore(dataDir + "/appointments.json");
    JsonFileStore documentsStore(dataDir + "/documents.json");
    JsonFileStore notificationsStore(dataDir + "/notifications.json");

    httplib::Server svr;

    // Serve the dashboard frontend directly so the browser and API share
    // an origin (no CORS setup needed for local/demo use).
    auto frontendDir = std::getenv("FRONTEND_DIR") ? std::string(std::getenv("FRONTEND_DIR")) : "../frontend";
    svr.set_mount_point("/", frontendDir);

    svr.Get("/api/patients", [&](const httplib::Request&, httplib::Response& res) {
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

    svr.Post("/api/backup", [&](const httplib::Request&, httplib::Response& res) {
        std::string path = storage.snapshotBackup();
        res.set_content(json{{"snapshot", path}}.dump(), "application/json");
    });

    // ---------------------------------------------------------------
    // Accounts. Passwords are salted-SHA256 (see PasswordHash.h) -- never
    // stored or returned in plaintext. There is no session token: the
    // frontend just holds the user object login/signup return and sends
    // the relevant id (patientId/doctorId/etc.) on later requests, the
    // same trust-the-caller model the existing /api/patients endpoints
    // already use.
    // ---------------------------------------------------------------

    svr.Post("/api/auth/signup", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            json body = json::parse(req.body);
            std::string name = body.value("name", "");
            std::string email = body.value("email", "");
            std::string password = body.value("password", "");
            std::string role = body.value("role", "");

            if (name.empty() || email.empty() || password.empty()) {
                res.status = 400;
                res.set_content(json{{"error", "name, email and password are required"}}.dump(), "application/json");
                return;
            }
            if (role != "Patient" && role != "Doctor" && role != "Nurse") {
                res.status = 400;
                res.set_content(json{{"error", "role must be Patient, Doctor or Nurse"}}.dump(), "application/json");
                return;
            }

            json all = usersStore.readAll();
            for (auto& u : all) {
                if (u.value("email", "") == email) {
                    res.status = 409;
                    res.set_content(json{{"error", "an account with this email already exists"}}.dump(), "application/json");
                    return;
                }
            }

            std::string salt = PasswordHash::randomSaltHex();
            std::string id = nextId("U-", all.size());
            json user{
                {"id", id},
                {"name", name},
                {"email", email},
                {"role", role},
                {"salt", salt},
                {"hash", PasswordHash::hash(password, salt)},
                {"createdAt", nowIso()}
            };
            usersStore.append(user);

            res.status = 201;
            res.set_content(publicUser(user).dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    svr.Post("/api/auth/login", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            json body = json::parse(req.body);
            std::string email = body.value("email", "");
            std::string password = body.value("password", "");

            json all = usersStore.readAll();
            for (auto& u : all) {
                if (u.value("email", "") != email) continue;
                std::string salt = u.value("salt", "");
                std::string expected = u.value("hash", "");
                if (PasswordHash::hash(password, salt) == expected) {
                    res.set_content(publicUser(u).dump(), "application/json");
                } else {
                    res.status = 401;
                    res.set_content(json{{"error", "invalid email or password"}}.dump(), "application/json");
                }
                return;
            }
            res.status = 401;
            res.set_content(json{{"error", "invalid email or password"}}.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    svr.Get("/api/users", [&](const httplib::Request& req, httplib::Response& res) {
        std::string role = req.has_param("role") ? req.get_param_value("role") : "";
        json all = usersStore.readAll();
        json out = json::array();
        for (auto& u : all) {
            if (!role.empty() && u.value("role", "") != role) continue;
            out.push_back(publicUser(u));
        }
        res.set_content(out.dump(), "application/json");
    });

    // ---------------------------------------------------------------
    // Doctor availability. A doctor posts the list of time slots they're
    // open for on a given date; patients read it back to pick one when
    // booking. Posting again for the same doctor+date merges by time so
    // an already-booked slot is never silently dropped or its booking
    // lost, even if the doctor re-saves their availability.
    // ---------------------------------------------------------------

    svr.Post("/api/availability", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            json body = json::parse(req.body);
            std::string doctorId = body.value("doctorId", "");
            std::string doctorName = body.value("doctorName", "");
            std::string date = body.value("date", "");

            if (doctorId.empty() || date.empty()) {
                res.status = 400;
                res.set_content(json{{"error", "doctorId and date are required"}}.dump(), "application/json");
                return;
            }

            std::vector<std::string> requestedTimes;
            for (auto& t : body.value("slots", json::array())) {
                if (t.is_string()) requestedTimes.push_back(t.get<std::string>());
            }

            json all = availabilityStore.readAll();
            int recordIndex = -1;
            json existingSlots = json::array();
            for (size_t i = 0; i < all.size(); ++i) {
                if (all[i].value("doctorId", "") == doctorId && all[i].value("date", "") == date) {
                    recordIndex = static_cast<int>(i);
                    existingSlots = all[i].value("slots", json::array());
                    break;
                }
            }

            json newSlots = json::array();
            for (auto& t : requestedTimes) {
                json kept;
                bool found = false;
                for (auto& es : existingSlots) {
                    if (es.value("time", "") == t) { kept = es; found = true; break; }
                }
                if (found) {
                    newSlots.push_back(kept);
                } else {
                    newSlots.push_back(json{{"time", t}, {"booked", false}, {"patientId", nullptr}, {"patientName", nullptr}});
                }
            }
            // A slot that's already booked stays on the list even if the
            // doctor's new submission dropped it.
            for (auto& es : existingSlots) {
                if (!es.value("booked", false)) continue;
                bool stillRequested = false;
                for (auto& t : requestedTimes) {
                    if (t == es.value("time", "")) { stillRequested = true; break; }
                }
                if (!stillRequested) newSlots.push_back(es);
            }

            json record{{"doctorId", doctorId}, {"doctorName", doctorName}, {"date", date}, {"slots", newSlots}};
            if (recordIndex >= 0) {
                all[static_cast<size_t>(recordIndex)] = record;
            } else {
                all.push_back(record);
            }
            availabilityStore.writeAll(all);
            res.set_content(record.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    svr.Get("/api/availability", [&](const httplib::Request& req, httplib::Response& res) {
        std::string doctorId = req.has_param("doctorId") ? req.get_param_value("doctorId") : "";
        std::string date = req.has_param("date") ? req.get_param_value("date") : "";
        json all = availabilityStore.readAll();
        json out = json::array();
        for (auto& rec : all) {
            if (!doctorId.empty() && rec.value("doctorId", "") != doctorId) continue;
            if (!date.empty() && rec.value("date", "") != date) continue;
            out.push_back(rec);
        }
        res.set_content(out.dump(), "application/json");
    });

    // ---------------------------------------------------------------
    // Appointments. Booking claims a specific open slot from
    // /api/availability (rejecting it if already taken) and fires a
    // notification to the patient, the doctor, and the nurse role at
    // large (appointments aren't assigned to a specific nurse).
    // ---------------------------------------------------------------

    auto pushNotification = [&](const std::string& forRole, const std::string& forUserId, const std::string& message) {
        json all = notificationsStore.readAll();
        json n{
            {"id", nextId("NTF-", all.size())},
            {"forRole", forRole},
            {"forUserId", forUserId},
            {"message", message},
            {"createdAt", nowIso()},
            {"read", false}
        };
        notificationsStore.append(n);
    };

    svr.Post("/api/appointments", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            json body = json::parse(req.body);
            std::string patientId = body.value("patientId", "");
            std::string patientName = body.value("patientName", "");
            std::string doctorId = body.value("doctorId", "");
            std::string doctorName = body.value("doctorName", "");
            std::string date = body.value("date", "");
            std::string time = body.value("time", "");
            std::string reason = body.value("reason", "");

            if (patientId.empty() || doctorId.empty() || date.empty() || time.empty()) {
                res.status = 400;
                res.set_content(json{{"error", "patientId, doctorId, date and time are required"}}.dump(), "application/json");
                return;
            }

            json avail = availabilityStore.readAll();
            int recordIndex = -1, slotIndex = -1;
            for (size_t i = 0; i < avail.size(); ++i) {
                if (avail[i].value("doctorId", "") != doctorId || avail[i].value("date", "") != date) continue;
                recordIndex = static_cast<int>(i);
                auto& slots = avail[i]["slots"];
                for (size_t j = 0; j < slots.size(); ++j) {
                    if (slots[j].value("time", "") == time) { slotIndex = static_cast<int>(j); break; }
                }
                break;
            }
            if (recordIndex < 0 || slotIndex < 0) {
                res.status = 404;
                res.set_content(json{{"error", "that slot doesn't exist"}}.dump(), "application/json");
                return;
            }
            auto& slot = avail[static_cast<size_t>(recordIndex)]["slots"][static_cast<size_t>(slotIndex)];
            if (slot.value("booked", false)) {
                res.status = 409;
                res.set_content(json{{"error", "that slot was just booked by someone else -- please pick another"}}.dump(), "application/json");
                return;
            }
            slot["booked"] = true;
            slot["patientId"] = patientId;
            slot["patientName"] = patientName;
            availabilityStore.writeAll(avail);

            json appointmentsAll = appointmentsStore.readAll();
            json appointment{
                {"id", nextId("APT-", appointmentsAll.size())},
                {"patientId", patientId},
                {"patientName", patientName},
                {"doctorId", doctorId},
                {"doctorName", doctorName},
                {"date", date},
                {"time", time},
                {"reason", reason},
                {"status", "Confirmed"},
                {"createdAt", nowIso()}
            };
            appointmentsStore.append(appointment);

            std::string summary = patientName + " with " + doctorName + " on " + date + " at " + time;
            pushNotification("Patient", patientId, "Appointment confirmed: " + summary);
            pushNotification("Doctor", doctorId, "New appointment booked: " + summary);
            pushNotification("Nurse", "", "New appointment scheduled: " + summary);

            res.status = 201;
            res.set_content(appointment.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    svr.Get("/api/appointments", [&](const httplib::Request& req, httplib::Response& res) {
        std::string patientId = req.has_param("patientId") ? req.get_param_value("patientId") : "";
        std::string doctorId = req.has_param("doctorId") ? req.get_param_value("doctorId") : "";
        json all = appointmentsStore.readAll();
        json out = json::array();
        for (auto& a : all) {
            if (!patientId.empty() && a.value("patientId", "") != patientId) continue;
            if (!doctorId.empty() && a.value("doctorId", "") != doctorId) continue;
            out.push_back(a);
        }
        res.set_content(out.dump(), "application/json");
    });

    // ---------------------------------------------------------------
    // Documents: files a patient uploads (txt/docx/pdf, base64-encoded in
    // the JSON body) and reports a nurse attaches for a patient (sent the
    // same way, as a small text file) -- both visible to the patient,
    // their doctor, and nursing staff. Listing omits the (potentially
    // large) file bytes; /api/documents/content fetches one file's bytes
    // by id, with the right Content-Type for viewing/downloading.
    // ---------------------------------------------------------------

    svr.Post("/api/documents", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            json body = json::parse(req.body);
            std::string patientId = body.value("patientId", "");
            std::string patientName = body.value("patientName", "");
            std::string uploaderRole = body.value("uploaderRole", "");
            std::string uploaderName = body.value("uploaderName", "");
            std::string filename = body.value("filename", "");
            std::string contentType = body.value("contentType", "application/octet-stream");
            std::string dataBase64 = body.value("dataBase64", "");
            std::string note = body.value("note", "");

            if (patientId.empty() || filename.empty() || dataBase64.empty()) {
                res.status = 400;
                res.set_content(json{{"error", "patientId, filename and dataBase64 are required"}}.dump(), "application/json");
                return;
            }

            json all = documentsStore.readAll();
            json doc{
                {"id", nextId("DOC-", all.size())},
                {"patientId", patientId},
                {"patientName", patientName},
                {"uploaderRole", uploaderRole},
                {"uploaderName", uploaderName},
                {"filename", filename},
                {"contentType", contentType},
                {"dataBase64", dataBase64},
                {"note", note},
                {"createdAt", nowIso()}
            };
            documentsStore.append(doc);

            json out = doc;
            out.erase("dataBase64");
            res.status = 201;
            res.set_content(out.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    svr.Get("/api/documents", [&](const httplib::Request& req, httplib::Response& res) {
        std::string patientId = req.has_param("patientId") ? req.get_param_value("patientId") : "";
        json all = documentsStore.readAll();
        json out = json::array();
        for (auto& d : all) {
            if (!patientId.empty() && d.value("patientId", "") != patientId) continue;
            json meta = d;
            meta.erase("dataBase64");
            out.push_back(meta);
        }
        res.set_content(out.dump(), "application/json");
    });

    svr.Get("/api/documents/content", [&](const httplib::Request& req, httplib::Response& res) {
        std::string id = req.has_param("id") ? req.get_param_value("id") : "";
        json all = documentsStore.readAll();
        for (auto& d : all) {
            if (d.value("id", "") != id) continue;
            try {
                std::string bytes = base64::decode(d.value("dataBase64", ""));
                res.set_content(bytes, d.value("contentType", "application/octet-stream"));
            } catch (const std::exception& e) {
                res.status = 500;
                res.set_content(json{{"error", e.what()}}.dump(), "application/json");
            }
            return;
        }
        res.status = 404;
        res.set_content(json{{"error", "document not found"}}.dump(), "application/json");
    });

    // ---------------------------------------------------------------
    // Notifications: created by pushNotification() above (currently only
    // on appointment booking). forUserId is empty for a role-wide
    // broadcast (used for the Nurse role, since appointments aren't
    // assigned to a specific nurse).
    // ---------------------------------------------------------------

    svr.Get("/api/notifications", [&](const httplib::Request& req, httplib::Response& res) {
        std::string role = req.has_param("role") ? req.get_param_value("role") : "";
        std::string userId = req.has_param("userId") ? req.get_param_value("userId") : "";
        json all = notificationsStore.readAll();
        json out = json::array();
        // Newest first.
        for (size_t i = all.size(); i-- > 0; ) {
            auto& n = all[i];
            if (n.value("forRole", "") != role) continue;
            std::string forUserId = n.value("forUserId", "");
            if (!forUserId.empty() && forUserId != userId) continue;
            out.push_back(n);
        }
        res.set_content(out.dump(), "application/json");
    });

    svr.Post("/api/notifications/read", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            json body = json::parse(req.body);
            std::string id = body.value("id", "");
            json all = notificationsStore.readAll();
            bool found = false;
            for (auto& n : all) {
                if (n.value("id", "") == id) {
                    n["read"] = true;
                    found = true;
                    break;
                }
            }
            if (!found) {
                res.status = 404;
                res.set_content(json{{"error", "notification not found"}}.dump(), "application/json");
                return;
            }
            notificationsStore.writeAll(all);
            res.set_content(json{{"ok", true}}.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    svr.Get("/api/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(json{{"status", "ok"}}.dump(), "application/json");
    });

    std::cout << "Patient dashboard API listening on http://localhost:8080\n";
    svr.listen("0.0.0.0", 8080);
    return 0;
}
