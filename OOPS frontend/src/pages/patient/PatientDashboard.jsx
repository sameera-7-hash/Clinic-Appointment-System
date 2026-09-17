import React, { useEffect, useState } from "react";
import { useCurrentUser } from "../../context/UserContext";
import NotificationBell from "../../components/NotificationBell";
import ThemeToggle from "../../components/ThemeToggle";
import { getAppointments, getDocuments, documentContentUrl } from "../../Services/api";

function PatientDashboard({ navigate, logout }) {
  const [profile, setProfile] = useState(false);
  const { user } = useCurrentUser();

  const [appointments, setAppointments] = useState([]);
  const [documents, setDocuments] = useState([]);

  useEffect(() => {
    if (!user) return;
    let cancelled = false;

    getAppointments({ patientId: user.id })
      .then((data) => { if (!cancelled) setAppointments(data); })
      .catch(() => {});

    getDocuments(user.id)
      .then((data) => { if (!cancelled) setDocuments(data); })
      .catch(() => {});

    return () => { cancelled = true; };
  }, [user]);

  // Soonest appointment by date+time, today or later.
  const today = new Date().toISOString().slice(0, 10);
  const upcoming = appointments
    .filter((a) => a.date >= today)
    .sort((a, b) => (a.date + a.time).localeCompare(b.date + b.time))[0];

  const recentDocuments = [...documents]
    .sort((a, b) => b.createdAt.localeCompare(a.createdAt))
    .slice(0, 3);

  return (
    <div className="dashboard-page">

      <header className="topbar">

        <div className="brand">
          <div className="brand-logo">+</div>

          <div>
            <h2>PatientCare</h2>
            <span>Secure Healthcare Management</span>
          </div>
        </div>


        <div className="topbar-right">

          <ThemeToggle />

          <NotificationBell role="Patient" userId={user?.id} />

          <button
            className="user-btn"
            onClick={() => setProfile(true)}
          >

            <div className="avatar">
              {user?.name ? user.name.charAt(0).toUpperCase() : "P"}
            </div>

            <div>
              <strong>{user?.name || "Patient"}</strong>
              <small>Patient</small>
            </div>

          </button>

        </div>

      </header>


      <div className="dashboard-body">

        <aside className="side-nav">

          <button
            className="nav-link active"
            onClick={() => navigate("patient-dashboard")}
          >
            🏠 Overview
          </button>

          <button
            className="nav-link"
            onClick={() => navigate("patient-appointments")}
          >
            📅 Appointments
          </button>

          <button
            className="nav-link"
            onClick={() => navigate("medical-records")}
          >
            📄 My Reports
          </button>

          <button
            className="nav-link"
            onClick={() => navigate("prescriptions")}
          >
            💊 Prescriptions
          </button>

          <button
            className="nav-link"
            onClick={() => navigate("medical-history")}
          >
            📋 Medical History
          </button>


          <div className="nav-bottom">

            <button
              className="nav-link"
              onClick={() => setProfile(true)}
            >
              👤 Profile
            </button>

            <button className="nav-link">
              ⚙ Settings
            </button>

            <button
              className="logout-btn"
              onClick={logout}
            >
              ↪ Logout
            </button>

          </div>

        </aside>


        <main className="dashboard-content">

          <div className="page-heading">

            <div>
              <span className="eyebrow">
                PATIENT PORTAL
              </span>

              <h1>
                Welcome, {user?.name || "Patient"} 👋
              </h1>

              <p>
                Take care of your health today.
              </p>
            </div>

          </div>


          <div className="stats-container">

            <div className="stat-box">
              <span>📅 Upcoming Appointment</span>
              <strong>{upcoming ? 1 : 0}</strong>
              <small>{upcoming ? `${upcoming.date}, ${upcoming.time}` : "None scheduled"}</small>
            </div>

            <div className="stat-box">
              <span>📄 Reports</span>
              <strong>{documents.length}</strong>
              <small>Available</small>
            </div>

            <div className="stat-box">
              <span>💊 Prescriptions</span>
              <strong>0</strong>
              <small>Active prescriptions</small>
            </div>

            <div className="stat-box">
              <span>❤️ Health Status</span>
              <strong className="success-text">
                Good
              </strong>
              <small>✓ Healthy</small>
            </div>

          </div>


          <div className="two-column">

            <section className="card">

              <div className="card-title">

                <div>
                  <h2>Upcoming Appointment</h2>
                  <p>Your next scheduled consultation</p>
                </div>

                {upcoming && (
                  <span className="status-badge">
                    {upcoming.status}
                  </span>
                )}

              </div>

              {upcoming ? (
                <div className="appointment-box">

                  <div className="doctor-avatar">
                    ⚕
                  </div>

                  <div>
                    <h3>{upcoming.doctorName}</h3>
                    <p>{upcoming.reason || "Consultation"}</p>
                  </div>

                  <div className="appointment-date">
                    <b>{upcoming.date}</b>
                    <span>{upcoming.time}</span>
                  </div>

                </div>
              ) : (
                <p className="dashboard-subtitle">No upcoming appointments yet.</p>
              )}


              <button
                className="primary-btn"
                onClick={() => navigate("patient-appointments")}
              >
                {upcoming ? "View Appointment Details →" : "Book an Appointment →"}
              </button>

            </section>


            <section className="card">

              <div className="card-title">

                <div>
                  <h2>Recent Reports</h2>
                  <p>Your latest medical reports</p>
                </div>

                <button
                  className="text-btn"
                  onClick={() => navigate("medical-records")}
                >
                  View All
                </button>

              </div>


              {recentDocuments.length === 0 && (
                <p className="dashboard-subtitle">No reports uploaded yet.</p>
              )}

              {recentDocuments.map((doc) => (
                <div className="report-item" key={doc.id}>
                  <span>📄</span>
                  <div>
                    <b>{doc.filename}</b>
                    <small>
                      {doc.uploaderRole === "Nurse" ? "From your nurse" : "You uploaded"} · {new Date(doc.createdAt).toLocaleDateString()}
                    </small>
                  </div>
                  <a href={documentContentUrl(doc.id)} target="_blank" rel="noreferrer">
                    <button>View</button>
                  </a>
                </div>
              ))}

            </section>

          </div>


          <div className="health-grid">

            <div className="health-box">
              <span>❤️</span>
              <div>
                <b>Health Status</b>
                <p>Your current health status is good.</p>
              </div>
              <strong className="success-text">
                ✓ Healthy
              </strong>
            </div>


            <div className="health-box">
              <span>🔐</span>
              <div>
                <b>Data Security</b>
                <p>Your health records are protected.</p>
              </div>
              <strong className="success-text">
                Protected
              </strong>
            </div>

          </div>


          <div className="security-strip">
            🔐 Your health data is encrypted and securely stored.
          </div>

        </main>

      </div>


      {profile && (
        <div
          className="modal-overlay"
          onClick={() => setProfile(false)}
        >

          <div
            className="profile-modal"
            onClick={(e) => e.stopPropagation()}
          >

            <button
              className="modal-close"
              onClick={() => setProfile(false)}
            >
              ×
            </button>

            <div className="large-avatar">
              {user?.name ? user.name.charAt(0).toUpperCase() : "P"}
            </div>

            <h2>{user?.name || "Patient"}</h2>
            <p>Patient</p>

            <div className="profile-info">

              <div>
                <span>Name</span>
                <b>{user?.name || "—"}</b>
              </div>

              <div>
                <span>Email</span>
                <b>{user?.email || "—"}</b>
              </div>

              <div>
                <span>Reports</span>
                <b>{documents.length} Available</b>
              </div>

              <div>
                <span>Health Status</span>
                <b className="success-text">
                  ● Good
                </b>
              </div>

            </div>

            <button
              className="primary-btn full-btn"
              onClick={() => setProfile(false)}
            >
              Close
            </button>

          </div>

        </div>
      )}

    </div>
  );
}

export default PatientDashboard;