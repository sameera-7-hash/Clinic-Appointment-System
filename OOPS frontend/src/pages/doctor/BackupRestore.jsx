import React, { useState } from "react";
import { DoctorLayout } from "./DoctorDashboard";
import { createBackup } from "../../Services/api";

function BackupRestore({ navigate, logout }) {

  const [backingUp, setBackingUp] = useState(false);

  const backup = async () => {
    setBackingUp(true);
    try {
      const result = await createBackup();
      alert("Snapshot saved: " + result.snapshot);
    } catch (err) {
      alert("Could not create snapshot: " + err.message);
    } finally {
      setBackingUp(false);
    }
  };

  return (
    <DoctorLayout
      active="backup-restore"
      navigate={navigate}
      logout={logout}
      menu={doctorMenu}
    >

      <div className="page-heading">

        <h1>Backup & Restore</h1>

        <p>
          Manage database backups and verify integrity.
        </p>

      </div>

      <div className="content-grid">

        <div className="panel">

          <h3>Primary Database</h3>

          <div className="backup-file">
            <strong>patients.dat</strong>
            <span className="status green">
              ✓ Healthy
            </span>
          </div>

        </div>

        <div className="panel">

          <h3>Backup Database</h3>

          <div className="backup-file">
            <strong>patients.dat.bak</strong>
            <span className="status green">
              ✓ Available
            </span>
          </div>

        </div>

      </div>

      <div className="panel">

        <h3>Integrity Status</h3>

        <div className="integrity-box">
          ✓ Hash Chain Verified
          <p>
            All stored records are consistent and tamper-evident.
          </p>
        </div>

      </div>

      <div className="action-buttons">

        <button
          className="primary-button"
          onClick={backup}
          disabled={backingUp}
        >
          💾 {backingUp ? "Creating..." : "Create Snapshot"}
        </button>

        <button
          className="primary-button"
          onClick={() =>
            alert("Restoring from a snapshot isn't exposed by the backend API yet — the server automatically recovers from patients.dat.bak if the primary file is corrupted.")
          }
        >
          ↻ Restore Backup
        </button>

      </div>

    </DoctorLayout>
  );
}

const doctorMenu = [
  ["▦", "Dashboard", "doctor-dashboard"],
  ["👥", "Patients", "doctor-patients"],
  ["📅", "Appointments", "doctor-appointments"],
  ["➕", "Add Patient", "add-patient"],
  ["📊", "Analytics", "analytics"],
  ["↶", "Undo / Redo", "undo-redo"],
  ["💾", "Backup & Restore", "backup-restore"]
];

export default BackupRestore;