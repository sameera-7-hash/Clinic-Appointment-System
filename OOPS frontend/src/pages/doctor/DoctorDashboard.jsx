import React, { useEffect, useState } from "react";
import { getPatients } from "../../Services/api";
import { useCurrentUser } from "../../context/UserContext";
import NotificationBell from "../../components/NotificationBell";
import ThemeToggle from "../../components/ThemeToggle";

function DoctorDashboard({ navigate, logout }) {

  const { user } = useCurrentUser();

  const menu = [
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

  const [patients, setPatients] = useState([]);
  const [chainIntact, setChainIntact] = useState(true);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    let cancelled = false;

    getPatients()
      .then((data) => {
        if (cancelled) return;
        setPatients(data.patients);
        setChainIntact(data.chainIntact);
      })
      .catch(() => {})
      .finally(() => {
        if (!cancelled) setLoading(false);
      });

    return () => {
      cancelled = true;
    };
  }, []);

  const okPatients = patients.filter((p) => p.ok);
  const recent = okPatients.slice(-5).reverse();

  return (
    <DoctorLayout
      menu={menu}
      active="doctor-dashboard"
      navigate={navigate}
      logout={logout}
    >

      <h1>Good Morning, {user ? user.name : "Doctor"} 👋</h1>

      <p className="dashboard-subtitle">
        Here's today's clinical overview.
      </p>

      <div className="stats-grid">

        <Stat
          title="Total Patients"
          value={loading ? "…" : String(okPatients.length)}
          note="Live from backend"
        />

        <Stat title="Today's Appointments" value="12" note="View schedule" />

        <Stat
          title="Legacy (DES) Records"
          value={loading ? "…" : String(okPatients.filter((p) => p.algo === "DES").length)}
          note="Recommend re-encrypting"
        />

        <Stat
          title="Integrity Status"
          value={loading ? "…" : chainIntact ? "OK" : "BROKEN"}
          note={chainIntact ? "All records secure" : "Tampering detected"}
        />

      </div>

      <div className="panel">

        <div className="panel-header">

          <h3>Recent Patients</h3>

          <button onClick={() => navigate("doctor-patients")}>
            View All
          </button>

        </div>

        <table className="data-table">

          <thead>
            <tr>
              <th>ID</th>
              <th>Name</th>
              <th>Age / Gender</th>
              <th>Diagnosis</th>
              <th>Encryption</th>
              <th>Integrity</th>
            </tr>
          </thead>

          <tbody>

            {!loading && recent.length === 0 && (
              <tr>
                <td colSpan={6}>No patients yet.</td>
              </tr>
            )}

            {recent.map((p) => (
              <PatientRow
                key={p.id}
                id={p.id}
                name={p.name}
                age={`${p.age} Y / ${p.gender}`}
                diagnosis={p.diagnosis}
                encryption={p.algo}
                ok={p.ok}
              />
            ))}

          </tbody>

        </table>

      </div>

      <div className="panel">

        <h3>Recent Actions</h3>

        <div className="quick-actions">

          <button onClick={() => navigate("add-patient")}>
            ➕ Add Patient
            <small>New patient record</small>
          </button>

          <button onClick={() => navigate("doctor-patients")}>
            🔍 Search Patient
            <small>Find patient quickly</small>
          </button>

          <button onClick={() => navigate("undo-redo")}>
            ↶ Undo
            <small>Undo last action</small>
          </button>

          <button onClick={() => navigate("undo-redo")}>
            ↷ Redo
            <small>Redo last action</small>
          </button>

        </div>

      </div>

      <div className="security-banner">
        🛡 AES-256 / DES Encryption &nbsp; | &nbsp;
        💾 Backup Available &nbsp; | &nbsp;
        {chainIntact ? "✓ Integrity Verified" : "⚠ Integrity BROKEN"}
      </div>

    </DoctorLayout>
  );
}

function Stat({ title, value, note }) {
  return (
    <div className="stat-card">
      <span>{title}</span>
      <strong>{value}</strong>
      <small>{note}</small>
    </div>
  );
}

function PatientRow({ id, name, age, diagnosis, encryption, ok }) {
  return (
    <tr>
      <td>{id}</td>
      <td>{name}</td>
      <td>{age}</td>
      <td>{diagnosis}</td>
      <td>{encryption}</td>
      <td>
        <span className={`status ${ok ? "green" : "red"}`}>
          {ok ? "✓ OK" : "⚠ Failed"}
        </span>
      </td>
    </tr>
  );
}

export function DoctorLayout({
  children,
  menu,
  active,
  navigate,
  logout
}) {

  const { user } = useCurrentUser();

  return (
    <div className="dashboard-layout">

      <aside className="sidebar doctor-sidebar">

        <div className="sidebar-brand">
          ✚ PatientCare
        </div>

        {menu.map((item) => (

          <button
            key={item[1]}
            className={
              active === item[2]
                ? "sidebar-item active"
                : "sidebar-item"
            }
            onClick={() => navigate(item[2])}
          >
            {item[0]} {item[1]}
          </button>

        ))}

        <div className="sidebar-bottom">

          <button className="sidebar-item">👤 Profile</button>

          <button className="sidebar-item">⚙ Settings</button>

          <button className="logout-button" onClick={logout}>
            ↪ Logout
          </button>

        </div>

      </aside>

      <main className="dashboard-main">

        <header className="dashboard-header">

          <h2>Doctor Dashboard</h2>

          <div className="user-profile">

            <ThemeToggle />

            <NotificationBell role="Doctor" userId={user?.id} />

            <div className="avatar doctor-avatar">
              {user?.name ? user.name.charAt(0).toUpperCase() : "D"}
            </div>

            <div>
              <strong>{user?.name || "Doctor"}</strong>
              <small>Doctor</small>
            </div>

          </div>

        </header>

        <section className="dashboard-content">
          {children}
        </section>

      </main>

    </div>
  );
}

export default DoctorDashboard;
