import React, { useEffect, useState } from "react";
import { DoctorLayout } from "./DoctorDashboard";
import { getPatients, getDocuments, documentContentUrl } from "../../Services/api";

function PatientDetails({ navigate, logout, patientId }) {

  const [patient, setPatient] = useState(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState("");
  const [documents, setDocuments] = useState([]);

  useEffect(() => {
    let cancelled = false;

    // The backend has no GET /api/patients/:id, so we fetch the full list
    // and find the record client-side.
    getPatients()
      .then((data) => {
        if (cancelled) return;
        const found = data.patients.find((p) => p.id === patientId);
        if (!found) {
          setError("Patient not found.");
        } else {
          setPatient(found);
        }
      })
      .catch((err) => {
        if (!cancelled) setError(err.message);
      })
      .finally(() => {
        if (!cancelled) setLoading(false);
      });

    if (patientId) {
      getDocuments(patientId)
        .then((data) => {
          if (!cancelled) setDocuments([...data].sort((a, b) => b.createdAt.localeCompare(a.createdAt)));
        })
        .catch(() => {});
    }

    return () => {
      cancelled = true;
    };
  }, [patientId]);

  return (
    <DoctorLayout
      active="doctor-patients"
      navigate={navigate}
      logout={logout}
      menu={doctorMenu}
    >

      <button
        className="back-link"
        onClick={() => navigate("doctor-patients")}
      >
        ← Back to Patients
      </button>

      <div className="page-heading">

        <h1>Patient Details</h1>

        <p>Complete patient information and medical history.</p>

      </div>

      {loading && <p className="dashboard-subtitle">Loading patient...</p>}

      {!loading && error && <p className="dashboard-subtitle">{error}</p>}

      {!loading && !error && patient && (
        <>
          <div className="content-grid">

            <div className="panel">

              <div className="patient-profile-large">

                <div className="large-avatar">
                  {patient.name ? patient.name.charAt(0).toUpperCase() : "?"}
                </div>

                <div>
                  <h2>{patient.name}</h2>
                  <p>{patient.id} • {patient.age} Years • {patient.gender}</p>
                </div>

              </div>

              <hr />

              <div className="details-grid">

                <Detail label="Contact Number" value={patient.contact || "—"} />

                <Detail label="Admission Date" value={patient.admissionDate || "—"} />

                <Detail label="Diagnosis" value={patient.diagnosis || "—"} />

                <Detail label="Encryption" value={patient.algo || "—"} />

              </div>

            </div>

            <div className="panel">

              <h3>Reports & Documents</h3>

              {documents.length === 0 && (
                <p className="dashboard-subtitle">No reports or uploaded files for this patient yet.</p>
              )}

              {documents.map((doc) => (
                <p key={doc.id}>
                  📄 <a href={documentContentUrl(doc.id)} target="_blank" rel="noreferrer">{doc.filename}</a>
                  {" "}— {doc.uploaderRole === "Nurse" ? `${doc.uploaderName} (nurse)` : "patient upload"}, {new Date(doc.createdAt).toLocaleDateString()}
                </p>
              ))}

              <h3>Prescriptions</h3>

              <p className="dashboard-subtitle">Not tracked by the backend yet.</p>

            </div>

          </div>

          <div className="action-buttons">

            <button className="primary-button">
              Edit Record
            </button>

            <button className="primary-button">
              Add Prescription
            </button>

            <button className="primary-button">
              Add Diagnosis
            </button>

          </div>

          <div className="security-banner">
            🔐 Encryption: {patient.algo || "unknown"} &nbsp; | &nbsp;
            {patient.ok ? "✓ Integrity Verified" : "⚠ Integrity check failed"} &nbsp; | &nbsp;
            💾 Backup Available
          </div>
        </>
      )}

    </DoctorLayout>
  );
}

function Detail({ label, value }) {

  return (
    <div className="detail-item">
      <small>{label}</small>
      <strong>{value}</strong>
    </div>
  );
}

const doctorMenu = [
  ["▦", "Dashboard", "doctor-dashboard"],
  ["👥", "Patients", "doctor-patients"],
  ["📅", "Appointments", "doctor-appointments"],
  ["➕", "Add Patient", "add-patient"],
  ["💊", "Prescriptions", "doctor-dashboard"],
  ["📄", "Reports", "doctor-dashboard"],
  ["📊", "Analytics", "analytics"],
  ["↶", "Undo / Redo", "undo-redo"],
  ["💾", "Backup & Restore", "backup-restore"]
];

export default PatientDetails;
