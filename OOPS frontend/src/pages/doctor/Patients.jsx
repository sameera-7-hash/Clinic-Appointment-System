import React, { useEffect, useState } from "react";
import { DoctorLayout } from "./DoctorDashboard";
import { getPatients } from "../../Services/api";

function Patients({ navigate, logout }) {

  const [search, setSearch] = useState("");
  const [sort, setSort] = useState("name");
  const [patients, setPatients] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState("");

  useEffect(() => {
    let cancelled = false;

    getPatients()
      .then((data) => {
        if (cancelled) return;
        setPatients(data.patients.filter((p) => p.ok));
      })
      .catch((err) => {
        if (!cancelled) setError(err.message);
      })
      .finally(() => {
        if (!cancelled) setLoading(false);
      });

    return () => {
      cancelled = true;
    };
  }, []);

  const filtered = patients
    .filter((p) =>
      `${p.id} ${p.name} ${p.diagnosis}`
        .toLowerCase()
        .includes(search.toLowerCase())
    )
    .sort((a, b) => {
      if (sort === "name") return a.name.localeCompare(b.name);
      if (sort === "age") return a.age - b.age;
      if (sort === "id") return a.id.localeCompare(b.id);
      return 0;
    });

  return (
    <DoctorLayout
      active="doctor-patients"
      navigate={navigate}
      logout={logout}
      menu={doctorMenu}
    >

      <div className="page-heading">

        <h1>Patient Records</h1>

        <p>
          Search, sort and manage patient information.
        </p>

      </div>

      <div className="panel">

        <div className="search-row">

          <input
            placeholder="🔍 Search by ID, name or diagnosis..."
            value={search}
            onChange={(e) => setSearch(e.target.value)}
          />

          <select
            value={sort}
            onChange={(e) => setSort(e.target.value)}
          >
            <option value="name">Sort by Name</option>
            <option value="age">Sort by Age</option>
            <option value="id">Sort by Patient ID</option>
          </select>

        </div>

        {loading && <p className="dashboard-subtitle">Loading patients...</p>}

        {error && <p className="dashboard-subtitle">Could not load patients: {error}</p>}

        {!loading && !error && (
          <table className="data-table">

            <thead>
              <tr>
                <th>ID</th>
                <th>Name</th>
                <th>Age</th>
                <th>Gender</th>
                <th>Diagnosis</th>
                <th>Encryption</th>
                <th>Action</th>
              </tr>
            </thead>

            <tbody>

              {filtered.length === 0 && (
                <tr>
                  <td colSpan={7}>No patients yet. Add the first one from "Add Patient".</td>
                </tr>
              )}

              {filtered.map((patient) => (

                <tr key={patient.id}>

                  <td>{patient.id}</td>
                  <td>{patient.name}</td>
                  <td>{patient.age}</td>
                  <td>{patient.gender}</td>
                  <td>{patient.diagnosis}</td>

                  <td>
                    <span className={`status ${patient.algo && patient.algo.startsWith("DES") ? "gray" : "green"}`}>
                      {patient.algo}
                    </span>
                  </td>

                  <td>
                    <button
                      className="small-button"
                      onClick={() => navigate("patient-details", patient.id)}
                    >
                      View
                    </button>
                  </td>

                </tr>

              ))}

            </tbody>

          </table>
        )}

      </div>

      <div className="dsainfo">
        🔎 Patient ID Search → Hash Map concept &nbsp; | &nbsp;
        ↕ Sorting → Sorting algorithm
      </div>

    </DoctorLayout>
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

export default Patients;
