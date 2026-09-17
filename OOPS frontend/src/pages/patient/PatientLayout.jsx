import React from "react";
import { useCurrentUser } from "../../context/UserContext";
import NotificationBell from "../../components/NotificationBell";
import ThemeToggle from "../../components/ThemeToggle";

function PatientLayout({
  children,
  title,
  active,
  navigate,
  logout
}) {

  const { user } = useCurrentUser();

  const menu = [
    ["⌂", "Overview", "patient-dashboard"],
    ["▣", "Appointments", "patient-appointments"],
    ["▤", "My Reports", "medical-records"],
    ["💊", "Prescriptions", "prescriptions"],
    ["▣", "Medical History", "medical-history"]
  ];

  return (
    <div className="patient-app">

      {/* ================= SIDEBAR ================= */}

      <aside className="patient-sidebar">

        <div className="patient-brand">

          <div className="patient-brand-icon">
            +
          </div>

          <div>
            <h2>PatientCare</h2>

            <span>
              Secure Healthcare Management
            </span>
          </div>

        </div>


        <nav className="patient-nav">

          {menu.map((item) => (

            <button
              key={item[1]}
              className={
                active === item[2]
                  ? "patient-nav-item active"
                  : "patient-nav-item"
              }
              onClick={() => navigate(item[2])}
            >

              <span className="patient-nav-icon">
                {item[0]}
              </span>

              <span>
                {item[1]}
              </span>

            </button>

          ))}

        </nav>


        <div className="patient-sidebar-bottom">

          <button className="patient-nav-item">

            <span className="patient-nav-icon">
              ♟
            </span>

            <span>
              Profile
            </span>

          </button>


          <button className="patient-nav-item">

            <span className="patient-nav-icon">
              ⚙
            </span>

            <span>
              Settings
            </span>

          </button>


          <button
            className="patient-logout"
            onClick={logout}
          >

            <span>
              ↪
            </span>

            <span>
              Logout
            </span>

          </button>

        </div>

      </aside>


      {/* ================= MAIN AREA ================= */}

      <div className="patient-main">

        <header className="patient-topbar">

          <div className="patient-topbar-title">

            <strong>
              {title}
            </strong>

          </div>


          <div className="patient-user-area">

            <ThemeToggle />

            <NotificationBell role="Patient" userId={user?.id} />


            <div className="patient-avatar">
              {user?.name ? user.name.charAt(0).toUpperCase() : "P"}
            </div>


            <div className="patient-user-info">

              <strong>
                {user?.name || "Patient"}
              </strong>

              <small>
                Patient
              </small>

            </div>

          </div>

        </header>


        {/* ================= PAGE CONTENT ================= */}

        <main className="patient-page-content">

          {children}

        </main>

      </div>

    </div>
  );
}

export default PatientLayout;