// Thin client for the C++ patient-records backend (patient_dashboard/backend).
// In dev, Vite proxies "/api" to http://localhost:8080 (see vite.config.js),
// so requests below always use same-origin relative paths.

const BASE = "/api";

async function request(path, options = {}) {
  let res;
  try {
    res = await fetch(`${BASE}${path}`, {
      headers: { "Content-Type": "application/json" },
      ...options,
    });
  } catch (err) {
    throw new Error(
      "Could not reach the backend. Is the patient_server running on port 8080?"
    );
  }

  const data = await res.json().catch(() => ({}));
  if (!res.ok) {
    throw new Error(data.error || `Request failed (${res.status})`);
  }
  return data;
}

// GET /api/patients -> { chainIntact, usedBackup, count, patients: [...] }
// Each patient record also carries `algo` (cipher used) and `ok` (integrity
// check result); when ok is false, `problem` explains what failed.
export function getPatients() {
  return request("/patients");
}

// POST /api/patients. `patient` should have: name, age, gender, contact,
// diagnosis, admissionDate, algo ("AES" | "DES"). The backend assigns `id`.
export function addPatient(patient) {
  return request("/patients", {
    method: "POST",
    body: JSON.stringify(patient),
  });
}

// POST /api/backup -> { snapshot: "<path to timestamped copy>" }
export function createBackup() {
  return request("/backup", { method: "POST" });
}

// GET /api/health -> { status: "ok" }
export function checkHealth() {
  return request("/health");
}

// ---------------------------------------------------------------
// Accounts. There is no session token -- login/signup return the user
// object {id, name, email, role} and the frontend just holds onto it
// (see UserContext), sending the relevant id on later requests.
// ---------------------------------------------------------------

export function signup({ name, email, password, role }) {
  return request("/auth/signup", {
    method: "POST",
    body: JSON.stringify({ name, email, password, role }),
  });
}

export function login({ email, password }) {
  return request("/auth/login", {
    method: "POST",
    body: JSON.stringify({ email, password }),
  });
}

// GET /api/users?role=Doctor -> [{id, name, email, role}, ...]
export function getUsers(role) {
  return request(`/users${role ? `?role=${encodeURIComponent(role)}` : ""}`);
}

// ---------------------------------------------------------------
// Doctor availability & appointments.
// ---------------------------------------------------------------

// Overwrites (merging by time) a doctor's open slots for one date.
// `slots` is an array of time strings, e.g. ["09:00 AM", "09:30 AM"].
export function setAvailability({ doctorId, doctorName, date, slots }) {
  return request("/availability", {
    method: "POST",
    body: JSON.stringify({ doctorId, doctorName, date, slots }),
  });
}

// GET /api/availability?date=&doctorId= -> array of
// { doctorId, doctorName, date, slots: [{time, booked, patientId, patientName}] }
export function getAvailability({ doctorId, date } = {}) {
  const params = new URLSearchParams();
  if (doctorId) params.set("doctorId", doctorId);
  if (date) params.set("date", date);
  const qs = params.toString();
  return request(`/availability${qs ? `?${qs}` : ""}`);
}

// Books a specific open slot. Throws if it was already taken.
export function bookAppointment({ patientId, patientName, doctorId, doctorName, date, time, reason }) {
  return request("/appointments", {
    method: "POST",
    body: JSON.stringify({ patientId, patientName, doctorId, doctorName, date, time, reason }),
  });
}

// GET /api/appointments?patientId= or ?doctorId= (omit both to get all --
// used by the nurse view, since appointments aren't assigned to one nurse).
export function getAppointments({ patientId, doctorId } = {}) {
  const params = new URLSearchParams();
  if (patientId) params.set("patientId", patientId);
  if (doctorId) params.set("doctorId", doctorId);
  const qs = params.toString();
  return request(`/appointments${qs ? `?${qs}` : ""}`);
}

// ---------------------------------------------------------------
// Documents: patient-uploaded files and nurse-authored reports, both
// visible to the patient, their doctor, and nursing staff.
// ---------------------------------------------------------------

// `dataBase64` is the raw file content (or report text) base64-encoded --
// see fileToBase64() below for turning a browser File into this shape.
export function uploadDocument({ patientId, patientName, uploaderRole, uploaderName, filename, contentType, dataBase64, note }) {
  return request("/documents", {
    method: "POST",
    body: JSON.stringify({ patientId, patientName, uploaderRole, uploaderName, filename, contentType, dataBase64, note }),
  });
}

// GET /api/documents?patientId= -> metadata only (no file bytes)
export function getDocuments(patientId) {
  return request(`/documents?patientId=${encodeURIComponent(patientId)}`);
}

// URL to fetch/view one document's raw bytes (open in a new tab, or use
// as a download link's href).
export function documentContentUrl(id) {
  return `${BASE}/documents/content?id=${encodeURIComponent(id)}`;
}

// Reads a browser File object into a base64 string (no "data:...;base64,"
// prefix) for uploadDocument()'s dataBase64 field.
export function fileToBase64(file) {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onload = () => {
      const result = reader.result;
      const commaIndex = result.indexOf(",");
      resolve(commaIndex >= 0 ? result.slice(commaIndex + 1) : result);
    };
    reader.onerror = () => reject(reader.error || new Error("Could not read file"));
    reader.readAsDataURL(file);
  });
}

// ---------------------------------------------------------------
// Notifications / reminders.
// ---------------------------------------------------------------

// GET /api/notifications?role=&userId= -> newest first. userId is
// optional (omit for a role-wide broadcast, used by the Nurse role).
export function getNotifications(role, userId) {
  const params = new URLSearchParams({ role });
  if (userId) params.set("userId", userId);
  return request(`/notifications?${params.toString()}`);
}

export function markNotificationRead(id) {
  return request("/notifications/read", {
    method: "POST",
    body: JSON.stringify({ id }),
  });
}
