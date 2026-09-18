import React, { useEffect, useState } from "react";
import { DoctorLayout } from "./DoctorDashboard";
import { useCurrentUser } from "../../context/UserContext";
import { getAvailability, setAvailability, getAppointments } from "../../Services/api";
import { Badge } from "../../components/ui/Badge";

function todayIso() {
  return new Date().toISOString().slice(0, 10);
}

function Appointments({ navigate, logout }) {

  const { user } = useCurrentUser();

  const [date, setDate] = useState(todayIso());
  const [slots, setSlots] = useState([]); // [{time, booked, patientName}]
  const [slotInput, setSlotInput] = useState("");
  const [savingAvailability, setSavingAvailability] = useState(false);
  const [loadingSlots, setLoadingSlots] = useState(true);

  const [appointments, setAppointments] = useState([]);
  const [loadingAppointments, setLoadingAppointments] = useState(true);

  useEffect(() => {
    if (!user) return;
    let cancelled = false;
    setLoadingSlots(true);

    getAvailability({ doctorId: user.id, date })
      .then((records) => {
        if (cancelled) return;
        setSlots(records[0]?.slots || []);
      })
      .catch(() => {})
      .finally(() => {
        if (!cancelled) setLoadingSlots(false);
      });

    return () => { cancelled = true; };
  }, [user, date]);

  useEffect(() => {
    if (!user) return;
    let cancelled = false;

    getAppointments({ doctorId: user.id })
      .then((data) => {
        if (cancelled) return;
        setAppointments([...data].sort((a, b) => (a.date + a.time).localeCompare(b.date + b.time)));
      })
      .catch(() => {})
      .finally(() => {
        if (!cancelled) setLoadingAppointments(false);
      });

    return () => { cancelled = true; };
  }, [user]);

  const addSlot = () => {
    const time = slotInput.trim();
    if (!time) return;
    if (slots.some((s) => s.time === time)) {
      setSlotInput("");
      return;
    }
    setSlots([...slots, { time, booked: false }]);
    setSlotInput("");
  };

  const removeSlot = (time) => {
    setSlots(slots.filter((s) => s.time !== time));
  };

  const saveAvailability = async () => {
    if (!user) return;
    setSavingAvailability(true);
    try {
      const record = await setAvailability({
        doctorId: user.id,
        doctorName: user.name,
        date,
        slots: slots.map((s) => s.time),
      });
      setSlots(record.slots || []);
      alert(`Availability saved for ${date}.`);
    } catch (err) {
      alert("Could not save availability: " + err.message);
    } finally {
      setSavingAvailability(false);
    }
  };

  return (
    <DoctorLayout
      active="doctor-appointments"
      navigate={navigate}
      logout={logout}
      menu={doctorMenu}
    >

      <div className="page-heading">

        <h1>Appointments</h1>

        <p>Set your availability and manage booked consultations.</p>

      </div>

      <div className="panel">

        <div className="panel-header">
          <h3>My Availability</h3>
        </div>

        <div className="availability-editor">

          <label>
            Date
            <input
              type="date"
              value={date}
              min={todayIso()}
              onChange={(e) => setDate(e.target.value)}
            />
          </label>

          <label>
            Add a time slot
            <div className="slot-input-row">
              <input
                type="text"
                placeholder="e.g. 09:00 AM"
                value={slotInput}
                onChange={(e) => setSlotInput(e.target.value)}
                onKeyDown={(e) => { if (e.key === "Enter") { e.preventDefault(); addSlot(); } }}
              />
              <button type="button" className="small-button" onClick={addSlot}>
                + Add
              </button>
            </div>
          </label>

        </div>

        {loadingSlots ? (
          <p className="dashboard-subtitle">Loading slots...</p>
        ) : (
          <div className="slot-chip-list">
            {slots.length === 0 && <p className="dashboard-subtitle">No slots set for this date yet.</p>}
            {slots.map((s) => (
              <span key={s.time} className={`slot-chip${s.booked ? " booked" : ""}`}>
                {s.time}
                {s.booked ? ` · booked (${s.patientName || "patient"})` : (
                  <button type="button" onClick={() => removeSlot(s.time)}>×</button>
                )}
              </span>
            ))}
          </div>
        )}

        <button
          className="primary-button"
          onClick={saveAvailability}
          disabled={savingAvailability}
        >
          {savingAvailability ? "Saving..." : "💾 Save Availability"}
        </button>

      </div>

      <div className="panel">

        <div className="panel-header">
          <h3>Booked Appointments</h3>
        </div>

        <table className="data-table">

          <thead>
            <tr>
              <th>Date</th>
              <th>Time</th>
              <th>Patient</th>
              <th>Reason</th>
              <th>Status</th>
            </tr>
          </thead>

          <tbody>

            {!loadingAppointments && appointments.length === 0 && (
              <tr>
                <td colSpan={5}>No appointments booked yet.</td>
              </tr>
            )}

            {appointments.map((a) => (
              <tr key={a.id}>
                <td>{a.date}</td>
                <td>{a.time}</td>
                <td>{a.patientName}</td>
                <td>{a.reason || "—"}</td>
                <td><Badge variant="success">{a.status}</Badge></td>
              </tr>
            ))}

          </tbody>

        </table>

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

export default Appointments;
