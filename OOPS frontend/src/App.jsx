import React, { useEffect, useState } from "react";
import { UserContext } from "./context/UserContext";

import Home from "./pages/Home";
import Login from "./pages/Login";
import RoleSelection from "./pages/RoleSelection";

import PatientDashboard from "./pages/patient/PatientDashboard";
import PatientAppointments from "./pages/patient/Appointments";
import MedicalRecords from "./pages/patient/MedicalRecords";
import Prescriptions from "./pages/patient/Prescriptions";
import MedicalHistory from "./pages/patient/MedicalHistory";

import DoctorDashboard from "./pages/doctor/DoctorDashboard";
import Patients from "./pages/doctor/Patients";
import AddPatient from "./pages/doctor/AddPatient";
import PatientDetails from "./pages/doctor/PatientDetails";
import DoctorAppointments from "./pages/doctor/Appointments";
import Analytics from "./pages/doctor/Analytics";
import UndoRedo from "./pages/doctor/UndoRedo";
import BackupRestore from "./pages/doctor/BackupRestore";

import NurseDashboard from "./pages/nurse/NurseDashboard";
import PatientQueue from "./pages/nurse/PatientQueue";
import PriorityQueue from "./pages/nurse/PriorityQueue";
import Vitals from "./pages/nurse/Vitals";
import Tasks from "./pages/nurse/Tasks";
import Alerts from "./pages/nurse/Alerts";
import NurseReports from "./pages/nurse/Reports";

function loadStoredUser() {
  try {
    const raw = localStorage.getItem("pc_user");
    return raw ? JSON.parse(raw) : null;
  } catch {
    return null;
  }
}

function dashboardPageFor(role) {
  if (role === "Patient") return "patient-dashboard";
  if (role === "Doctor") return "doctor-dashboard";
  if (role === "Nurse") return "nurse-dashboard";
  return "home";
}

function App() {
  const [user, setUser] = useState(loadStoredUser);
  const [page, setPage] = useState(() => (loadStoredUser() ? dashboardPageFor(loadStoredUser().role) : "home"));
  const [role, setRole] = useState(() => loadStoredUser()?.role || null);
  const [selectedPatientId, setSelectedPatientId] = useState(null);

  // Applied once so the chosen theme is visible even on the Home/Login
  // screens, not just after a dashboard (with its own ThemeToggle) mounts.
  useEffect(() => {
    try {
      document.documentElement.setAttribute("data-theme", localStorage.getItem("pc_theme") || "light");
    } catch {
      // ignore
    }
  }, []);

  const selectRole = (selectedRole) => {
    setRole(selectedRole);
    setPage("login");
  };

  // Called by Login with the user object /api/auth/login or
  // /api/auth/signup returned.
  const onAuthenticated = (authedUser) => {
    setUser(authedUser);
    try {
      localStorage.setItem("pc_user", JSON.stringify(authedUser));
    } catch {
      // Private browsing / blocked storage -- session just won't persist.
    }
    setPage(dashboardPageFor(authedUser.role));
  };

  const logout = () => {
    setUser(null);
    setRole(null);
    try {
      localStorage.removeItem("pc_user");
    } catch {
      // ignore
    }
    setPage("home");
  };

  // `payload` currently only carries a patient id (from Patients -> View),
  // used by PatientDetails to look up the record it should display.
  const navigate = (newPage, payload) => {
    if (newPage === "patient-details" && payload) {
      setSelectedPatientId(payload);
    }
    setPage(newPage);
  };

  const renderPage = () => {
    if (page === "home") {
      return <Home onLogin={() => setPage("role-selection")} />;
    }

    if (page === "role-selection") {
      return <RoleSelection onSelectRole={selectRole} onBack={() => setPage("home")} />;
    }

    if (page === "login") {
      return <Login role={role} onAuthenticated={onAuthenticated} onBack={() => setPage("home")} />;
    }

    /* ================= PATIENT ================= */

    if (page === "patient-dashboard") {
      return <PatientDashboard navigate={navigate} logout={logout} />;
    }

    if (page === "patient-appointments") {
      return <PatientAppointments navigate={navigate} logout={logout} />;
    }

    if (page === "medical-records") {
      return <MedicalRecords navigate={navigate} logout={logout} />;
    }

    if (page === "prescriptions") {
      return <Prescriptions navigate={navigate} logout={logout} />;
    }

    if (page === "medical-history") {
      return <MedicalHistory navigate={navigate} logout={logout} />;
    }

    /* ================= DOCTOR ================= */

    if (page === "doctor-dashboard") {
      return <DoctorDashboard navigate={navigate} logout={logout} />;
    }

    if (page === "doctor-patients") {
      return <Patients navigate={navigate} logout={logout} />;
    }

    if (page === "add-patient") {
      return <AddPatient navigate={navigate} logout={logout} />;
    }

    if (page === "patient-details") {
      return <PatientDetails navigate={navigate} logout={logout} patientId={selectedPatientId} />;
    }

    if (page === "doctor-appointments") {
      return <DoctorAppointments navigate={navigate} logout={logout} />;
    }

    if (page === "analytics") {
      return <Analytics navigate={navigate} logout={logout} />;
    }

    if (page === "undo-redo") {
      return <UndoRedo navigate={navigate} logout={logout} />;
    }

    if (page === "backup-restore") {
      return <BackupRestore navigate={navigate} logout={logout} />;
    }

    /* ================= NURSE ================= */

    if (page === "nurse-dashboard") {
      return <NurseDashboard navigate={navigate} logout={logout} />;
    }

    if (page === "patient-queue") {
      return <PatientQueue navigate={navigate} logout={logout} />;
    }

    if (page === "priority-queue") {
      return <PriorityQueue navigate={navigate} logout={logout} />;
    }

    if (page === "vitals") {
      return <Vitals navigate={navigate} logout={logout} />;
    }

    if (page === "tasks") {
      return <Tasks navigate={navigate} logout={logout} />;
    }

    if (page === "alerts") {
      return <Alerts navigate={navigate} logout={logout} />;
    }

    if (page === "nurse-reports") {
      return <NurseReports navigate={navigate} logout={logout} />;
    }

    return null;
  };

  return (
    <UserContext.Provider value={{ user, logout }}>
      {renderPage()}
    </UserContext.Provider>
  );
}

export default App;
