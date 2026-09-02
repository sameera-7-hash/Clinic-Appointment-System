/*
    ============================================================
    CLINIC APPOINTMENT MANAGEMENT SYSTEM
    ============================================================
    Full OOP C++ implementation based on the project plan:

    UI      -> Choose Doctor / Patient -> Clinic System (CLI menu)
    Logic   -> Appointment Management (Schedule/Slots, Waiting Queue,
               History/Reports)
    Data    -> Patient(User), Doctor(User), File storage
               (Doctor ID, Patient ID, Prescription, Total Expenses,
                Bill/Report)
    Status  -> After booking: Reschedule / Deadline / Reject / Confirm
    Booking -> Exact flowchart logic (Start -> Input -> Doctor exists? ->
               Valid date/time & in schedule? -> Doctor already booked? ->
               Create Appointment -> Save Record + Confirmation -> End)
    Stack   -> STL: vector, map, deque(queue), algorithm (sort, find_if)

    OOP concepts demonstrated:
      - Abstraction   : abstract base class Person (pure virtual function)
      - Inheritance   : Doctor and Patient derive from Person
      - Polymorphism  : virtual displayInfo() overridden in derived classes
      - Encapsulation : private/protected data with public getters/setters

    Compile:
        g++ -std=c++17 -O2 -o clinic ClinicAppointmentSystem.cpp
    Run:
        ./clinic
    ============================================================
*/

#include <iostream>
#include <vector>
#include <map>
#include <deque>
#include <string>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <limits>
#include <ctime>

// Portable directory creation without <filesystem>, since some compilers
// (older GCC/MinGW, some IDEs like Dev-C++/Code::Blocks default setups)
// either predate C++17 or need an extra -lstdc++fs link flag we can't rely on.
#if defined(_WIN32)
    #include <direct.h>
    #define CLINIC_MKDIR(dir) _mkdir(dir)
#else
    #include <sys/stat.h>
    #include <sys/types.h>
    #define CLINIC_MKDIR(dir) mkdir(dir, 0755)
#endif

using namespace std;

// Creates the directory if it doesn't already exist. If it already exists,
// mkdir/_mkdir simply fails and we ignore that - not an error for our purposes.
static void ensureDirectoryExists(const string &dir) {
    CLINIC_MKDIR(dir.c_str());
}

// ------------------------------------------------------------
// Utility helpers
// ------------------------------------------------------------
static void pause() {
    // Every read function above already consumes its own trailing newline,
    // so at this point the stream is positioned right at the start of the
    // next line. A single get() is enough to wait for / consume the user's
    // Enter key press without eating the first character of what follows.
    cout << "\nPress Enter to continue...";
    cin.get();
}

static int readInt(const string &prompt) {
    int value;
    while (true) {
        cout << prompt;
        if (cin >> value) {
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            return value;
        }
        if (cin.eof()) { cout << "\nInput stream closed. Exiting.\n"; exit(0); }
        cout << "Invalid input. Please enter a number.\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
}

static string readLine(const string &prompt) {
    string value;
    cout << prompt;
    if (!getline(cin, value)) { cout << "\nInput stream closed. Exiting.\n"; exit(0); }
    return value;
}

static double readDouble(const string &prompt) {
    double value;
    while (true) {
        cout << prompt;
        if (cin >> value) {
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            return value;
        }
        if (cin.eof()) { cout << "\nInput stream closed. Exiting.\n"; exit(0); }
        cout << "Invalid input. Please enter a number.\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
}

// today's date as YYYY-MM-DD, used to timestamp uploaded reports automatically
static string currentDateString() {
    time_t t = time(nullptr);
    tm *lt = localtime(&t); // plain localtime() - simplest, works on every compiler
    ostringstream oss;
    oss << put_time(lt, "%Y-%m-%d");
    return oss.str();
}

// simple split on delimiter, used for file storage (CSV-like with '|')
static vector<string> split(const string &s, char delim) {
    vector<string> tokens;
    stringstream ss(s);
    string item;
    while (getline(ss, item, delim)) tokens.push_back(item);
    return tokens;
}

// ------------------------------------------------------------
// Appointment status (After appointment is booked -> ...)
// ------------------------------------------------------------
enum class AppointmentStatus { PENDING, CONFIRMED, REJECTED, RESCHEDULED, COMPLETED };

static string statusToString(AppointmentStatus s) {
    switch (s) {
        case AppointmentStatus::PENDING:     return "Pending";
        case AppointmentStatus::CONFIRMED:   return "Confirmed";
        case AppointmentStatus::REJECTED:    return "Rejected";
        case AppointmentStatus::RESCHEDULED: return "Rescheduled";
        case AppointmentStatus::COMPLETED:   return "Completed";
    }
    return "Unknown";
}

static AppointmentStatus stringToStatus(const string &s) {
    if (s == "Confirmed")   return AppointmentStatus::CONFIRMED;
    if (s == "Rejected")    return AppointmentStatus::REJECTED;
    if (s == "Rescheduled") return AppointmentStatus::RESCHEDULED;
    if (s == "Completed")   return AppointmentStatus::COMPLETED;
    return AppointmentStatus::PENDING;
}

// ------------------------------------------------------------
// Abstraction: Person (abstract base class)
// ------------------------------------------------------------
class Person {
protected:
    int id;
    string name;
    int age;
    string contact;

public:
    Person(int id_, const string &name_, int age_, const string &contact_)
        : id(id_), name(name_), age(age_), contact(contact_) {}

    virtual ~Person() = default;

    int getId() const { return id; }
    string getName() const { return name; }
    int getAge() const { return age; }
    string getContact() const { return contact; }

    // Pure virtual -> forces every derived class to define its own info display
    virtual void displayInfo() const = 0;

    // Priority rule from the notes: age < 10 or age > 60 => priority patient
    bool isPriority() const { return age < 10 || age > 60; }
};

// ------------------------------------------------------------
// Doctor : public Person
// ------------------------------------------------------------
class Doctor : public Person {
private:
    string specialization;
    double fee;
    // availability: date -> list of open time slots
    map<string, vector<string>> availability;

public:
    Doctor(int id_, const string &name_, int age_, const string &contact_,
           const string &specialization_, double fee_)
        : Person(id_, name_, age_, contact_), specialization(specialization_), fee(fee_) {}

    string getSpecialization() const { return specialization; }
    double getFee() const { return fee; }
    const map<string, vector<string>>& getAvailability() const { return availability; }

    void addSlot(const string &date, const string &time) {
        auto &slots = availability[date];
        if (find(slots.begin(), slots.end(), time) == slots.end())
            slots.push_back(time);
    }

    // Decision 2 of flowchart: "Valid date/time & in schedule?"
    bool isSlotOpen(const string &date, const string &time) const {
        auto it = availability.find(date);
        if (it == availability.end()) return false;
        const auto &slots = it->second;
        return find(slots.begin(), slots.end(), time) != slots.end();
    }

    // Remove a slot once it's booked (so it can't be double-booked)
    void removeSlot(const string &date, const string &time) {
        auto it = availability.find(date);
        if (it == availability.end()) return;
        auto &slots = it->second;
        slots.erase(remove(slots.begin(), slots.end(), time), slots.end());
    }

    // Re-open a slot if an appointment is cancelled/rejected
    void freeSlot(const string &date, const string &time) {
        addSlot(date, time);
    }

    void displayInfo() const override {
        cout << "  [Doctor] ID:" << id << "  Name: " << name
             << "  Age:" << age << "  Contact:" << contact
             << "  Specialization:" << specialization
             << "  Fee: Rs." << fixed << setprecision(2) << fee << "\n";
    }

    void displayAvailability() const {
        if (availability.empty()) {
            cout << "    No availability set.\n";
            return;
        }
        for (const auto &entry : availability) {
            cout << "    " << entry.first << " -> ";
            if (entry.second.empty()) { cout << "(fully booked)"; }
            for (const auto &t : entry.second) cout << t << "  ";
            cout << "\n";
        }
    }
};

// ------------------------------------------------------------
// Patient : public Person
// ------------------------------------------------------------
class Patient : public Person {
private:
    vector<int> appointmentHistory; // appointment IDs
    vector<string> prescriptions;
    double totalExpenses;

public:
    Patient(int id_, const string &name_, int age_, const string &contact_)
        : Person(id_, name_, age_, contact_), totalExpenses(0.0) {}

    void addAppointmentToHistory(int appointmentId) { appointmentHistory.push_back(appointmentId); }
    const vector<int>& getHistory() const { return appointmentHistory; }

    void addPrescription(const string &p) { if (!p.empty()) prescriptions.push_back(p); }
    const vector<string>& getPrescriptions() const { return prescriptions; }

    void addExpense(double amount) { totalExpenses += amount; }
    double getTotalExpenses() const { return totalExpenses; }
    void setTotalExpenses(double v) { totalExpenses = v; }

    void displayInfo() const override {
        cout << "  [Patient] ID:" << id << "  Name: " << name
             << "  Age:" << age << "  Contact:" << contact
             << (isPriority() ? "  (PRIORITY)" : "")
             << "  Total Expenses: Rs." << fixed << setprecision(2) << totalExpenses << "\n";
    }
};

// ------------------------------------------------------------
// Appointment
// ------------------------------------------------------------
class Appointment {
private:
    int id;
    int patientId;
    int doctorId;
    string date;
    string time;
    AppointmentStatus status;
    string prescription;
    double fee;

public:
    Appointment(int id_, int patientId_, int doctorId_, const string &date_,
                const string &time_, double fee_, AppointmentStatus status_ = AppointmentStatus::PENDING)
        : id(id_), patientId(patientId_), doctorId(doctorId_), date(date_), time(time_),
          status(status_), prescription(""), fee(fee_) {}

    int getId() const { return id; }
    int getPatientId() const { return patientId; }
    int getDoctorId() const { return doctorId; }
    string getDate() const { return date; }
    string getTime() const { return time; }
    double getFee() const { return fee; }
    AppointmentStatus getStatus() const { return status; }
    string getPrescription() const { return prescription; }

    void setStatus(AppointmentStatus s) { status = s; }
    void setDate(const string &d) { date = d; }
    void setTime(const string &t) { time = t; }
    void setPrescription(const string &p) { prescription = p; }

    void display() const {
        cout << "    Appt#" << id << " | Patient:" << patientId << " | Doctor:" << doctorId
             << " | " << date << " " << time << " | Status: " << statusToString(status)
             << " | Fee: Rs." << fixed << setprecision(2) << fee;
        if (!prescription.empty()) cout << " | Rx: " << prescription;
        cout << "\n";
    }
};

// ------------------------------------------------------------
// Report - a "previous report" belonging to a patient (e.g. a lab
// result, scan report, or clinical note). The patient supplies it
// either by typing it in directly or by importing an existing file
// from disk; the actual content is stored as its own file under the
// reports/ folder, and this class just holds the metadata record.
// ------------------------------------------------------------
class Report {
private:
    int id;
    int patientId;
    string title;
    string dateAdded;
    string filename; // path to the stored content file under reports/

public:
    Report(int id_, int patientId_, const string &title_, const string &dateAdded_, const string &filename_)
        : id(id_), patientId(patientId_), title(title_), dateAdded(dateAdded_), filename(filename_) {}

    int getId() const { return id; }
    int getPatientId() const { return patientId; }
    string getTitle() const { return title; }
    string getDateAdded() const { return dateAdded; }
    string getFilename() const { return filename; }
};

// ------------------------------------------------------------
// ClinicSystem - the CLI / menu driven core (UI + Logic + Data)
// ------------------------------------------------------------
class ClinicSystem {
private:
    vector<Doctor> doctors;
    vector<Patient> patients;
    vector<Appointment> appointments;

    // Waiting Queue: patient IDs waiting for a slot. Priority patients
    // (age < 10 or > 60) are inserted at the front (deque used as a
    // priority-aware queue, per the "Priority" note in the plan).
    deque<int> waitingQueue;

    // Previous Reports: patient-supplied files/notes (lab results, scans,
    // clinical notes, etc). Metadata lives in reportsIndexFile; actual
    // content is stored as individual files inside reportsDir.
    vector<Report> reports;

    int nextDoctorId = 1;
    int nextPatientId = 1;
    int nextAppointmentId = 1;
    int nextReportId = 1;

    const string doctorFile = "doctors.txt";
    const string patientFile = "patients.txt";
    const string appointmentFile = "appointments.txt";
    const string reportsIndexFile = "reports_index.txt";
    const string reportsDir = "reports";

public:
    ClinicSystem() { ensureDirectoryExists(reportsDir); loadData(); }
    ~ClinicSystem() { saveData(); }

    // ---------------- File storage (Data layer) ----------------
    void loadData() {
        ifstream df(doctorFile);
        if (df) {
            string line;
            while (getline(df, line)) {
                if (line.empty()) continue;
                auto t = split(line, '|');
                if (t.size() < 5) continue;
                Doctor d(stoi(t[0]), t[1], stoi(t[2]), t[3], t[4], t.size() > 5 ? stod(t[5]) : 0.0);
                // availability slots stored after index 6 as date:time;date:time...
                if (t.size() > 6 && !t[6].empty()) {
                    auto slots = split(t[6], ';');
                    for (auto &s : slots) {
                        auto ds = split(s, ':');
                        if (ds.size() == 2) d.addSlot(ds[0], ds[1]);
                    }
                }
                doctors.push_back(d);
                nextDoctorId = max(nextDoctorId, d.getId() + 1);
            }
        }

        ifstream pf(patientFile);
        if (pf) {
            string line;
            while (getline(pf, line)) {
                if (line.empty()) continue;
                auto t = split(line, '|');
                if (t.size() < 4) continue;
                Patient p(stoi(t[0]), t[1], stoi(t[2]), t[3]);
                if (t.size() > 4 && !t[4].empty()) p.setTotalExpenses(stod(t[4]));
                patients.push_back(p);
                nextPatientId = max(nextPatientId, p.getId() + 1);
            }
        }

        ifstream af(appointmentFile);
        if (af) {
            string line;
            while (getline(af, line)) {
                if (line.empty()) continue;
                auto t = split(line, '|');
                if (t.size() < 7) continue;
                Appointment a(stoi(t[0]), stoi(t[1]), stoi(t[2]), t[3], t[4], stod(t[5]), stringToStatus(t[6]));
                if (t.size() > 7) a.setPrescription(t[7]);
                appointments.push_back(a);
                nextAppointmentId = max(nextAppointmentId, a.getId() + 1);
                // rebuild patient history
                for (auto &p : patients)
                    if (p.getId() == a.getPatientId()) p.addAppointmentToHistory(a.getId());
            }
        }

        ifstream rf(reportsIndexFile);
        if (rf) {
            string line;
            while (getline(rf, line)) {
                if (line.empty()) continue;
                auto t = split(line, '|');
                if (t.size() < 5) continue;
                Report r(stoi(t[0]), stoi(t[1]), t[2], t[3], t[4]);
                reports.push_back(r);
                nextReportId = max(nextReportId, r.getId() + 1);
            }
        }
    }

    void saveData() {
        ofstream df(doctorFile, ios::trunc);
        for (const auto &d : doctors) {
            df << d.getId() << "|" << d.getName() << "|" << d.getAge() << "|" << d.getContact()
               << "|" << d.getSpecialization() << "|" << d.getFee() << "|";
            bool first = true;
            for (const auto &entry : d.getAvailability())
                for (const auto &t : entry.second) {
                    if (!first) df << ";";
                    df << entry.first << ":" << t;
                    first = false;
                }
            df << "\n";
        }

        ofstream pf(patientFile, ios::trunc);
        for (const auto &p : patients) {
            pf << p.getId() << "|" << p.getName() << "|" << p.getAge() << "|" << p.getContact()
               << "|" << p.getTotalExpenses() << "\n";
        }

        ofstream af(appointmentFile, ios::trunc);
        for (const auto &a : appointments) {
            af << a.getId() << "|" << a.getPatientId() << "|" << a.getDoctorId() << "|" << a.getDate()
               << "|" << a.getTime() << "|" << a.getFee() << "|" << statusToString(a.getStatus())
               << "|" << a.getPrescription() << "\n";
        }

        ofstream rf(reportsIndexFile, ios::trunc);
        for (const auto &r : reports) {
            rf << r.getId() << "|" << r.getPatientId() << "|" << r.getTitle() << "|"
               << r.getDateAdded() << "|" << r.getFilename() << "\n";
        }
    }

    // ---------------- Helper lookups (Searching with STL) ----------------
    Doctor* findDoctorById(int id) {
        auto it = find_if(doctors.begin(), doctors.end(), [&](const Doctor &d) { return d.getId() == id; });
        return it == doctors.end() ? nullptr : &(*it);
    }
    Patient* findPatientById(int id) {
        auto it = find_if(patients.begin(), patients.end(), [&](const Patient &p) { return p.getId() == id; });
        return it == patients.end() ? nullptr : &(*it);
    }
    Appointment* findAppointmentById(int id) {
        auto it = find_if(appointments.begin(), appointments.end(), [&](const Appointment &a) { return a.getId() == id; });
        return it == appointments.end() ? nullptr : &(*it);
    }

    // Agewise discount from the top notes: <10 or >60 => 20% discount, priority
    double calculateFeeAfterDiscount(double fee, int age) const {
        if (age < 10 || age > 60) return fee * 0.80;
        return fee;
    }

    // ---------------- Registration ----------------
    void registerDoctor() {
        cout << "\n--- Register Doctor ---\n";
        string name = readLine("Name: ");
        int age = readInt("Age: ");
        string contact = readLine("Contact: ");
        string spec = readLine("Specialization: ");
        double fee = readDouble("Consultation Fee: ");
        Doctor d(nextDoctorId++, name, age, contact, spec, fee);
        doctors.push_back(d);
        saveData();
        cout << "Doctor registered successfully. Doctor ID = " << d.getId() << "\n";
    }

    void registerPatient() {
        cout << "\n--- Register Patient ---\n";
        string name = readLine("Name: ");
        int age = readInt("Age: ");
        string contact = readLine("Contact: ");
        Patient p(nextPatientId++, name, age, contact);
        patients.push_back(p);
        saveData();
        cout << "Patient registered successfully. Patient ID = " << p.getId() << "\n";
        if (p.isPriority()) cout << "Note: You qualify for PRIORITY status and a 20% age-wise discount.\n";
    }

    // ================================================================
    // BOOKING FLOWCHART -- implemented exactly as specified:
    // START -> Enter Patient, Doctor, Date, Time -> Doctor exists? (No->Error)
    // -> Valid date/time & in schedule? (No->Reject)
    // -> Doctor already booked / none available? (Yes->Reject)
    // -> Create Appointment -> Save Record + Show Confirmation -> END
    // ================================================================
    void bookAppointment() {
        cout << "\n=== START: Book Appointment ===\n";

        int patientId = readInt("Enter Patient ID: ");
        int doctorId = readInt("Enter Doctor ID: ");
        string date = readLine("Enter Date (YYYY-MM-DD): ");
        string time = readLine("Enter Time (HH:MM): ");

        Patient *patient = findPatientById(patientId);
        if (!patient) {
            cout << "ERROR: Patient ID not found. Please register first.\n";
            return; // Error branch
        }

        // Decision 1: Doctor exists?
        Doctor *doctor = findDoctorById(doctorId);
        if (!doctor) {
            cout << "ERROR: Doctor does not exist.\n";
            return; // -> Error
        }

        // Decision 2: Valid date/time & in schedule?
        if (!doctor->isSlotOpen(date, time)) {
            cout << "REJECTED: That date/time is not in Dr. " << doctor->getName()
                 << "'s schedule.\n";
            offerWaitingQueue(patient->getId());
            return; // -> Reject
        }

        // Decision 3: Doctor already booked / none available?
        bool alreadyBooked = any_of(appointments.begin(), appointments.end(), [&](const Appointment &a) {
            return a.getDoctorId() == doctorId && a.getDate() == date && a.getTime() == time &&
                   a.getStatus() != AppointmentStatus::REJECTED;
        });
        if (alreadyBooked) {
            cout << "REJECTED: Doctor is already booked at that slot.\n";
            offerWaitingQueue(patient->getId());
            return; // -> Reject
        }

        // ---- Process: Create Appointment ----
        double finalFee = calculateFeeAfterDiscount(doctor->getFee(), patient->getAge());
        Appointment appt(nextAppointmentId++, patientId, doctorId, date, time, finalFee,
                          AppointmentStatus::CONFIRMED);
        doctor->removeSlot(date, time); // slot consumed
        appointments.push_back(appt);
        patient->addAppointmentToHistory(appt.getId());
        patient->addExpense(finalFee);

        // ---- Process: Save Record + Show Confirmation ----
        saveData();
        cout << "\n----------------------------------------\n";
        cout << " APPOINTMENT CONFIRMED\n";
        cout << "----------------------------------------\n";
        appt.display();
        if (patient->isPriority())
            cout << " (Priority patient - 20% age-wise discount applied)\n";
        cout << "----------------------------------------\n";
        cout << "=== END ===\n";
    }

    // If booking failed, optionally add the patient to the waiting queue.
    // Priority patients (age <10 or >60) go to the front of the queue.
    void offerWaitingQueue(int patientId) {
        string choice = readLine("Add yourself to the Waiting Queue for this doctor? (y/n): ");
        if (choice.empty() || (choice[0] != 'y' && choice[0] != 'Y')) return;

        Patient *p = findPatientById(patientId);
        if (p && p->isPriority()) {
            waitingQueue.push_front(patientId);
            cout << "Added to FRONT of waiting queue (priority patient).\n";
        } else {
            waitingQueue.push_back(patientId);
            cout << "Added to waiting queue.\n";
        }
    }

    void processWaitingQueue() {
        cout << "\n--- Waiting Queue ---\n";
        if (waitingQueue.empty()) {
            cout << "Queue is empty.\n";
            return;
        }
        cout << "Current queue order (front = next served):\n";
        int pos = 1;
        for (int pid : waitingQueue) {
            Patient *p = findPatientById(pid);
            cout << "  " << pos++ << ". Patient ID " << pid
                 << (p ? (" - " + p->getName() + (p->isPriority() ? " (PRIORITY)" : "")) : " - (unknown)")
                 << "\n";
        }
        string choice = readLine("Pop the next patient from the queue and attempt booking? (y/n): ");
        if (!choice.empty() && (choice[0] == 'y' || choice[0] == 'Y')) {
            int pid = waitingQueue.front();
            waitingQueue.pop_front();
            cout << "Now booking for Patient ID " << pid << "...\n";
            cout << "(Enter the doctor/date/time for this patient)\n";
            int doctorId = readInt("Enter Doctor ID: ");
            string date = readLine("Enter Date (YYYY-MM-DD): ");
            string time = readLine("Enter Time (HH:MM): ");

            Doctor *doctor = findDoctorById(doctorId);
            Patient *patient = findPatientById(pid);
            if (!doctor || !patient) { cout << "ERROR: Invalid doctor or patient.\n"; return; }
            if (!doctor->isSlotOpen(date, time)) { cout << "REJECTED: Slot not available.\n"; return; }

            double finalFee = calculateFeeAfterDiscount(doctor->getFee(), patient->getAge());
            Appointment appt(nextAppointmentId++, pid, doctorId, date, time, finalFee, AppointmentStatus::CONFIRMED);
            doctor->removeSlot(date, time);
            appointments.push_back(appt);
            patient->addAppointmentToHistory(appt.getId());
            patient->addExpense(finalFee);
            saveData();
            cout << "Appointment CONFIRMED for queued patient.\n";
            appt.display();
        }
    }

    // ---------------- Post-booking status actions ----------------
    // (Reschedule / Deadline / Reject / Confirm)
    void rescheduleAppointment() {
        int apptId = readInt("Enter Appointment ID to reschedule: ");
        Appointment *a = findAppointmentById(apptId);
        if (!a) { cout << "Appointment not found.\n"; return; }
        Doctor *doctor = findDoctorById(a->getDoctorId());
        if (!doctor) { cout << "Doctor record missing.\n"; return; }

        // free the old slot
        doctor->freeSlot(a->getDate(), a->getTime());

        string newDate = readLine("New Date (YYYY-MM-DD): ");
        string newTime = readLine("New Time (HH:MM): ");
        if (!doctor->isSlotOpen(newDate, newTime)) {
            cout << "REJECTED: New slot is not available. Re-booking old slot.\n";
            doctor->removeSlot(a->getDate(), a->getTime()); // restore
            return;
        }
        doctor->removeSlot(newDate, newTime);
        a->setDate(newDate);
        a->setTime(newTime);
        a->setStatus(AppointmentStatus::RESCHEDULED);
        saveData();
        cout << "Appointment RESCHEDULED successfully.\n";
        a->display();
    }

    void rejectOrCancelAppointment() {
        int apptId = readInt("Enter Appointment ID to reject/cancel: ");
        Appointment *a = findAppointmentById(apptId);
        if (!a) { cout << "Appointment not found.\n"; return; }
        Doctor *doctor = findDoctorById(a->getDoctorId());
        if (doctor) doctor->freeSlot(a->getDate(), a->getTime());
        a->setStatus(AppointmentStatus::REJECTED);
        saveData();
        cout << "Appointment REJECTED/CANCELLED. Slot freed for other patients.\n";

        if (!waitingQueue.empty()) {
            cout << "There are patients in the waiting queue - check 'Waiting Queue' menu to serve them.\n";
        }
    }

    void confirmAppointment() {
        int apptId = readInt("Enter Appointment ID to confirm: ");
        Appointment *a = findAppointmentById(apptId);
        if (!a) { cout << "Appointment not found.\n"; return; }
        a->setStatus(AppointmentStatus::CONFIRMED);
        saveData();
        cout << "Appointment CONFIRMED.\n";
    }

    void markCompletedWithDeadline() {
        int apptId = readInt("Enter Appointment ID to mark completed (deadline reached): ");
        Appointment *a = findAppointmentById(apptId);
        if (!a) { cout << "Appointment not found.\n"; return; }
        a->setStatus(AppointmentStatus::COMPLETED);
        saveData();
        cout << "Appointment marked COMPLETED (deadline passed).\n";
    }

    // ---------------- Doctor-side operations ----------------
    void addDoctorAvailability() {
        int docId = readInt("Enter your Doctor ID: ");
        Doctor *d = findDoctorById(docId);
        if (!d) { cout << "Doctor not found.\n"; return; }
        string date = readLine("Add availability for Date (YYYY-MM-DD): ");
        int count = readInt("How many time slots to add? ");
        for (int i = 0; i < count; ++i) {
            string t = readLine("  Time slot #" + to_string(i + 1) + " (HH:MM): ");
            d->addSlot(date, t);
        }
        saveData();
        cout << "Availability updated.\n";
    }

    void viewDoctorAvailability() {
        int docId = readInt("Enter Doctor ID to view availability: ");
        Doctor *d = findDoctorById(docId);
        if (!d) { cout << "Doctor not found.\n"; return; }
        d->displayInfo();
        d->displayAvailability();
    }

    void writePrescriptionAndBill() {
        int apptId = readInt("Enter Appointment ID: ");
        Appointment *a = findAppointmentById(apptId);
        if (!a) { cout << "Appointment not found.\n"; return; }
        string rx = readLine("Enter Prescription: ");
        a->setPrescription(rx);
        Patient *p = findPatientById(a->getPatientId());
        if (p) p->addPrescription(rx);
        a->setStatus(AppointmentStatus::COMPLETED);
        saveData();

        cout << "\n--- BILL / REPORT ---\n";
        cout << "Appointment ID : " << a->getId() << "\n";
        cout << "Patient ID     : " << a->getPatientId() << "\n";
        cout << "Doctor ID      : " << a->getDoctorId() << "\n";
        cout << "Prescription   : " << a->getPrescription() << "\n";
        cout << "Fee Charged    : Rs." << fixed << setprecision(2) << a->getFee() << "\n";
        if (p) cout << "Patient Total Expenses So Far: Rs." << fixed << setprecision(2)
                     << p->getTotalExpenses() << "\n";
        cout << "----------------------\n";
    }

    void viewPatientHistoryForDoctor() {
        int patId = readInt("Enter Patient ID to view history/past visits: ");
        Patient *p = findPatientById(patId);
        if (!p) { cout << "Patient not found.\n"; return; }
        p->displayInfo();
        cout << "  History / Past Visits:\n";
        for (int id : p->getHistory()) {
            Appointment *a = findAppointmentById(id);
            if (a) a->display();
        }
        if (!p->getPrescriptions().empty()) {
            cout << "  Previous Prescriptions:\n";
            for (const auto &rx : p->getPrescriptions()) cout << "    - " << rx << "\n";
        }
        // Also show any previous reports the patient has uploaded/typed in
        viewPreviousReports(patId);
    }

    // ---------------- Previous Reports (patient-supplied) ----------------
    // The patient can either type report content directly, or import an
    // existing file from disk (e.g. a lab report, scan, or PDF/text file).
    // The content is stored under reportsDir/ and indexed in reportsIndexFile
    // so it can later be listed and displayed to both the doctor and the
    // patient (satisfies the "Previous reports of patient" data point).
    void addPreviousReport() {
        cout << "\n--- Add Previous Report ---\n";
        int patientId = readInt("Enter your Patient ID: ");
        Patient *p = findPatientById(patientId);
        if (!p) { cout << "Patient not found. Please register first.\n"; return; }

        string title = readLine("Short title/description (e.g. 'Blood Test - Jan 2026'): ");
        // '|' is our file-storage delimiter, so strip it out of free-text fields
        title.erase(remove(title.begin(), title.end(), '|'), title.end());
        if (title.empty()) title = "Untitled Report";

        cout << "How would you like to add this report?\n";
        cout << "  1. Type the report content directly\n";
        cout << "  2. Import an existing file from disk (give its file path)\n";
        int mode = readInt("Choose: ");
        if (mode != 1 && mode != 2) {
            cout << "Invalid choice (must be 1 or 2). Report not added.\n";
            return;
        }

        int reportId = nextReportId; // reserved, only committed on success
        string storedFilename;

        if (mode == 2) {
            string srcPath = readLine("Enter the full path of the file to import: ");
            ifstream src(srcPath, ios::binary);
            if (!src) {
                cout << "Could not open that file (check the path). Report not added.\n";
                return;
            }
            string ext;
            auto dotPos = srcPath.find_last_of('.');
            auto slashPos = srcPath.find_last_of("/\\");
            bool dotIsInFilename = (dotPos != string::npos) &&
                                    (slashPos == string::npos || dotPos > slashPos);
            if (dotIsInFilename) ext = srcPath.substr(dotPos);
            storedFilename = reportsDir + "/patient_" + to_string(patientId) + "_report_" + to_string(reportId) + ext;
            ofstream dst(storedFilename, ios::binary);
            if (!dst) { cout << "Could not create stored report file. Report not added.\n"; return; }
            dst << src.rdbuf();
            cout << "File imported and saved as a previous report.\n";
        } else { // mode == 1
            storedFilename = reportsDir + "/patient_" + to_string(patientId) + "_report_" + to_string(reportId) + ".txt";
            ofstream dst(storedFilename);
            if (!dst) { cout << "Could not create report file. Report not added.\n"; return; }
            cout << "Type/paste the report content below.\n";
            cout << "Enter a single line containing only END when you are finished:\n";
            string line;
            while (true) {
                if (!getline(cin, line)) break; // stream closed
                if (line == "END") break;
                dst << line << "\n";
            }
            cout << "Report saved.\n";
        }

        nextReportId++;
        Report r(reportId, patientId, title, currentDateString(), storedFilename);
        reports.push_back(r);
        saveData();
        cout << "Previous report recorded for Patient ID " << patientId
             << " (Report ID " << reportId << ", dated " << r.getDateAdded() << ").\n";
        cout << "Tip: remember Patient ID " << patientId
             << " when you go to 'View My Previous Reports' later.\n";
    }

    // Lists and optionally displays a patient's previous reports.
    // Pass a specific patientId to skip the prompt (used when called from
    // the doctor's "view patient history" flow); pass -1 to ask for it.
    void viewPreviousReports(int patientIdParam = -1) {
        int patientId = patientIdParam;
        if (patientId == -1) patientId = readInt("Enter Patient ID to view previous reports for: ");
        Patient *p = findPatientById(patientId);
        if (!p) { cout << "Patient not found.\n"; return; }

        vector<Report*> mine;
        for (auto &r : reports) if (r.getPatientId() == patientId) mine.push_back(&r);

        cout << "  Previous Reports:\n";
        if (mine.empty()) {
            cout << "    No previous reports on file for Patient ID " << patientId << ".\n";
            // Helpful hint: show which patient IDs actually do have reports,
            // in case the wrong ID was typed (easy to do right after seeing
            // a freshly-created Report ID on screen).
            vector<int> patientsWithReports;
            for (const auto &r : reports) {
                if (find(patientsWithReports.begin(), patientsWithReports.end(), r.getPatientId())
                    == patientsWithReports.end())
                    patientsWithReports.push_back(r.getPatientId());
            }
            if (!patientsWithReports.empty()) {
                sort(patientsWithReports.begin(), patientsWithReports.end());
                cout << "    (Reports exist for Patient ID(s): ";
                for (size_t i = 0; i < patientsWithReports.size(); ++i) {
                    cout << patientsWithReports[i];
                    if (i + 1 < patientsWithReports.size()) cout << ", ";
                }
                cout << " - double check you entered the right Patient ID.)\n";
            }
            return;
        }
        for (auto *r : mine) {
            cout << "    Report ID " << r->getId() << " | " << r->getDateAdded()
                 << " | " << r->getTitle() << " | file: " << r->getFilename() << "\n";
        }

        string choice = readLine("Enter a Report ID to view its content (or press Enter to skip): ");
        if (choice.empty()) return;
        int rid;
        try { rid = stoi(choice); } catch (...) { cout << "Invalid Report ID.\n"; return; }

        auto it = find_if(reports.begin(), reports.end(),
                           [&](const Report &r) { return r.getId() == rid && r.getPatientId() == patientId; });
        if (it == reports.end()) { cout << "Report not found for this patient.\n"; return; }

        ifstream in(it->getFilename());
        if (!in) { cout << "Could not open the stored report file on disk.\n"; return; }
        cout << "\n----- Report Content: " << it->getTitle() << " (" << it->getDateAdded() << ") -----\n";
        string line;
        bool printedAny = false;
        while (getline(in, line)) { cout << line << "\n"; printedAny = true; }
        if (!printedAny) cout << "(File appears to be binary or empty - open " << it->getFilename()
                               << " directly with an appropriate viewer.)\n";
        cout << "----- End of Report -----\n";
    }

    // ---------------- Patient-side operations ----------------
    void viewOwnHistory() {
        int patId = readInt("Enter your Patient ID: ");
        Patient *p = findPatientById(patId);
        if (!p) { cout << "Patient not found.\n"; return; }
        p->displayInfo();
        cout << "  Appointment History:\n";
        for (int id : p->getHistory()) {
            Appointment *a = findAppointmentById(id);
            if (a) a->display();
        }
    }

    void viewFeesAndDoctors() {
        cout << "\n--- Doctors, Specializations & Fees ---\n";
        for (const auto &d : doctors) d.displayInfo();
    }

    // ---------------- Admin / Reports (sorting + searching demo) ----------------
    void generateReports() {
        cout << "\n=== CLINIC REPORT ===\n";
        cout << "Total Doctors : " << doctors.size() << "\n";
        cout << "Total Patients: " << patients.size() << "\n";
        cout << "Total Appointments: " << appointments.size() << "\n";

        double totalRevenue = 0;
        for (const auto &a : appointments)
            if (a.getStatus() == AppointmentStatus::CONFIRMED || a.getStatus() == AppointmentStatus::COMPLETED)
                totalRevenue += a.getFee();
        cout << "Total Revenue (Confirmed/Completed): Rs." << fixed << setprecision(2) << totalRevenue << "\n";

        // Sorting demo: appointments sorted by date then time
        vector<Appointment> sorted = appointments;
        sort(sorted.begin(), sorted.end(), [](const Appointment &a, const Appointment &b) {
            if (a.getDate() != b.getDate()) return a.getDate() < b.getDate();
            return a.getTime() < b.getTime();
        });
        cout << "\nAll Appointments (sorted by date/time):\n";
        for (const auto &a : sorted) a.display();

        // Searching demo: doctors sorted by fee (built-in algorithm usage)
        vector<Doctor> byFee = doctors;
        sort(byFee.begin(), byFee.end(), [](const Doctor &a, const Doctor &b) { return a.getFee() < b.getFee(); });
        cout << "\nDoctors sorted by fee (lowest first):\n";
        for (const auto &d : byFee) d.displayInfo();
    }

    void viewAllAppointments() {
        cout << "\n--- All Appointments ---\n";
        for (const auto &a : appointments) a.display();
    }

    // ---------------- Menus (UI layer) ----------------
    void doctorMenu() {
        int choice;
        do {
            cout << "\n===== DOCTOR MENU =====\n";
            cout << "1. Register New Doctor\n";
            cout << "2. Add Availability (Schedule/Slots)\n";
            cout << "3. View My Availability\n";
            cout << "4. View Patient History / Past Visits & Previous Reports\n";
            cout << "5. Write Prescription + Generate Bill\n";
            cout << "6. View All Appointments\n";
            cout << "7. Confirm an Appointment\n";
            cout << "8. Reschedule an Appointment\n";
            cout << "9. Reject / Cancel an Appointment\n";
            cout << "10. Mark Appointment Completed (Deadline)\n";
            cout << "11. Process Waiting Queue\n";
            cout << "0. Back to Main Menu\n";
            choice = readInt("Choose: ");
            switch (choice) {
                case 1: registerDoctor(); break;
                case 2: addDoctorAvailability(); break;
                case 3: viewDoctorAvailability(); break;
                case 4: viewPatientHistoryForDoctor(); break;
                case 5: writePrescriptionAndBill(); break;
                case 6: viewAllAppointments(); break;
                case 7: confirmAppointment(); break;
                case 8: rescheduleAppointment(); break;
                case 9: rejectOrCancelAppointment(); break;
                case 10: markCompletedWithDeadline(); break;
                case 11: processWaitingQueue(); break;
                case 0: break;
                default: cout << "Invalid choice.\n";
            }
            if (choice != 0) pause();
        } while (choice != 0);
    }

    void patientMenu() {
        int choice;
        do {
            cout << "\n===== PATIENT MENU =====\n";
            cout << "1. Register New Patient\n";
            cout << "2. Book Appointment\n";
            cout << "3. View My History / Past Visits\n";
            cout << "4. View Doctor Availability & Fees\n";
            cout << "5. Reschedule My Appointment\n";
            cout << "6. Reject / Cancel My Appointment\n";
            cout << "7. View Waiting Queue\n";
            cout << "8. Add a Previous Report (type notes or import a file)\n";
            cout << "9. View My Previous Reports\n";
            cout << "0. Back to Main Menu\n";
            choice = readInt("Choose: ");
            switch (choice) {
                case 1: registerPatient(); break;
                case 2: bookAppointment(); break;
                case 3: viewOwnHistory(); break;
                case 4: viewFeesAndDoctors(); break;
                case 5: rescheduleAppointment(); break;
                case 6: rejectOrCancelAppointment(); break;
                case 7: processWaitingQueue(); break;
                case 8: addPreviousReport(); break;
                case 9: viewPreviousReports(); break;
                case 0: break;
                default: cout << "Invalid choice.\n";
            }
            if (choice != 0) pause();
        } while (choice != 0);
    }

    void mainMenu() {
        int choice;
        do {
            cout << "\n############################################\n";
            cout << "   CLINIC APPOINTMENT MANAGEMENT SYSTEM\n";
            cout << "############################################\n";
            cout << "Choose User Type:\n";
            cout << "1. Doctor\n";
            cout << "2. Patient\n";
            cout << "3. Admin Reports (Satisfaction of patient + growth of hospital)\n";
            cout << "0. Exit\n";
            choice = readInt("Choose: ");
            switch (choice) {
                case 1: doctorMenu(); break;
                case 2: patientMenu(); break;
                case 3: generateReports(); pause(); break;
                case 0: cout << "Saving data and exiting. Goodbye!\n"; break;
                default: cout << "Invalid choice.\n";
            }
        } while (choice != 0);
    }
};

// ------------------------------------------------------------
// main()
// ------------------------------------------------------------
int main() {
    ClinicSystem clinic;
    clinic.mainMenu();
    return 0;
}