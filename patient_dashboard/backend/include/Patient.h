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
