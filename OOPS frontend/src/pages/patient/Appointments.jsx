import React, { useEffect, useState } from "react";
import PatientLayout from "./PatientLayout";
import { useCurrentUser } from "../../context/UserContext";
import { getAppointments, getAvailability, bookAppointment } from "../../Services/api";

function todayIso() {
  return new Date().toISOString().slice(0, 10);
}

function Appointments({ navigate, logout }) {
  const { user } = useCurrentUser();

  const [appointments, setAppointments] = useState([]);
  const [loading, setLoading] = useState(true);

  // Booking flow: null -> "asking" (yes/no) -> "picking" (date/doctor/slot/confirm).
  const [bookingStage, setBookingStage] = useState(null);
  const [bookingDate, setBookingDate] = useState(todayIso());
  const [doctorOptions, setDoctorOptions] = useState([]); // [{doctorId, doctorName, slots}]
  const [loadingSlots, setLoadingSlots] = useState(false);
  const [chosen, setChosen] = useState(null); // {doctorId, doctorName, time}
  const [reason, setReason] = useState("");
  const [booking, setBooking] = useState(false);

  const loadAppointments = () => {
    if (!user) return;
    setLoading(true);
    getAppointments({ patientId: user.id })
      .then((data) => setAppointments([...data].sort((a, b) => (a.date + a.time).localeCompare(b.date + b.time))))
      .catch(() => {})
      .finally(() => setLoading(false));
  };

  useEffect(loadAppointments, [user]);

  useEffect(() => {
    if (bookingStage !== "picking") return;
    setLoadingSlots(true);
    setChosen(null);
    getAvailability({ date: bookingDate })
      .then((records) => setDoctorOptions(records))
      .catch(() => setDoctorOptions([]))
      .finally(() => setLoadingSlots(false));
  }, [bookingStage, bookingDate]);

  const startBooking = () => {
    setBookingStage("asking");
  };

  const cancelBooking = () => {
    setBookingStage(null);
    setChosen(null);
    setReason("");
  };

  const confirmBooking = async () => {
    if (!chosen || !user) return;
    setBooking(true);
    try {
      await bookAppointment({
        patientId: user.id,
        patientName: user.name,
        doctorId: chosen.doctorId,
        doctorName: chosen.doctorName,
        date: bookingDate,
        time: chosen.time,
        reason,
      });
      alert("Appointment booked! You, your doctor and the nursing staff have been notified.");
      cancelBooking();
      loadAppointments();
    } catch (err) {
      alert("Could not book that slot: " + err.message);
      // The slot may have just been taken by someone else -- refresh the list.
      getAvailability({ date: bookingDate }).then(setDoctorOptions).catch(() => {});
    } finally {
      setBooking(false);
    }
  };

  return (
    <PatientLayout
      title="Appointments"
      active="patient-appointments"
      navigate={navigate}
      logout={logout}
    >

      <div className="patient-page-heading">

        <div>
          <span className="patient-eyebrow">
            PATIENT PORTAL
          </span>

          <h1>Appointments</h1>

          <p>View and manage your appointments.</p>
        </div>

        {!bookingStage && (
          <button className="book-appointment-btn" onClick={startBooking}>
            + Book Appointment
          </button>
        )}

      </div>

      {bookingStage === "asking" && (
        <section className="panel booking-flow">
          <div className="booking-ask">
            <p><strong>Would you like to book an appointment?</strong></p>
            <div className="booking-ask-buttons">
              <button className="primary-button" onClick={() => setBookingStage("picking")}>Yes, book one</button>
              <button className="small-button" onClick={cancelBooking}>No, not now</button>
            </div>
          </div>
        </section>
      )}

      {bookingStage === "picking" && (
        <section className="panel booking-flow">

          <label>
            Date
            <input
              type="date"
              value={bookingDate}
              min={todayIso()}
              onChange={(e) => setBookingDate(e.target.value)}
            />
          </label>

          {loadingSlots && <p className="dashboard-subtitle">Loading available doctors...</p>}

          {!loadingSlots && doctorOptions.length === 0 && (
            <p className="dashboard-subtitle">No doctors have posted availability for this date yet. Try another date.</p>
          )}

          {!loadingSlots && doctorOptions.map((doc) => {
            const openSlots = (doc.slots || []).filter((s) => !s.booked);
            if (openSlots.length === 0) return null;
            return (
              <div className="doctor-slot-block" key={doc.doctorId}>
                <h4>{doc.doctorName}</h4>
                <div className="slot-chip-list">
                  {openSlots.map((s) => (
                    <button
                      type="button"
                      key={s.time}
                      className={`slot-chip selectable${chosen && chosen.doctorId === doc.doctorId && chosen.time === s.time ? " selected" : ""}`}
                      onClick={() => setChosen({ doctorId: doc.doctorId, doctorName: doc.doctorName, time: s.time })}
                    >
                      {s.time}
                    </button>
                  ))}
                </div>
              </div>
            );
          })}

          {chosen && (
            <>
              <label>
                Reason for visit
                <textarea
                  placeholder="Briefly describe why you're booking this appointment"
                  value={reason}
                  onChange={(e) => setReason(e.target.value)}
                />
              </label>

              <p className="dashboard-subtitle">
                Booking with <strong>{chosen.doctorName}</strong> on <strong>{bookingDate}</strong> at <strong>{chosen.time}</strong>.
              </p>

              <button className="primary-button" onClick={confirmBooking} disabled={booking}>
                {booking ? "Booking..." : "Confirm Booking"}
              </button>
            </>
          )}

          <button className="small-button" onClick={cancelBooking}>Cancel</button>

        </section>
      )}


      <section className="appointment-section">

        {!loading && appointments.length === 0 && (
          <p className="dashboard-subtitle">You have no appointments yet.</p>
        )}

        {appointments.map((a) => {
          const [year, month, day] = a.date.split("-");
          return (
            <AppointmentCard
              key={a.id}
              day={day}
              date={`${a.date}`}
              time={a.time}
              doctor={a.doctorName}
              specialty={a.reason || "Consultation"}
              status={a.status}
            />
          );
        })}

      </section>

    </PatientLayout>
  );
}


function AppointmentCard({
  day,
  date,
  time,
  doctor,
  specialty,
  status
}) {
  return (
    <div className="appointment-modern-card">

      <div className="appointment-date-block">

        <strong>{day}</strong>

        <span>{date}</span>

        <small>{time}</small>

      </div>


      <div className="appointment-doctor-icon">
        ♡
      </div>


      <div className="appointment-doctor-info">

        <h3>{doctor}</h3>

        <p>{specialty}</p>

        <span
          className={
            status === "Pending"
              ? "appointment-status pending"
              : "appointment-status confirmed"
          }
        >
          {status}
        </span>

      </div>

    </div>
  );
}

export default Appointments;
