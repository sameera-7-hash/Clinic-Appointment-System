import React, { useEffect, useState } from "react";
import { NurseLayout } from "./NurseDashboard";
import { useCurrentUser } from "../../context/UserContext";
import { getPatients, getDocuments, uploadDocument, fileToBase64, documentContentUrl } from "../../Services/api";

function textToBase64(text) {
  // btoa only handles Latin1; encodeURIComponent/unescape round-trip gets
  // arbitrary UTF-8 text into that range safely.
  return btoa(unescape(encodeURIComponent(text)));
}

function Reports({ navigate, logout }) {

  const { user } = useCurrentUser();

  const [patients, setPatients] = useState([]);
  const [patientId, setPatientId] = useState("");
  const [reportText, setReportText] = useState("");
  const [file, setFile] = useState(null);
  const [submitting, setSubmitting] = useState(false);

  const [patientDocs, setPatientDocs] = useState([]);
  const [loadingDocs, setLoadingDocs] = useState(false);

  useEffect(() => {
    getPatients()
      .then((data) => setPatients(data.patients.filter((p) => p.ok)))
      .catch(() => {});
  }, []);

  useEffect(() => {
    if (!patientId) { setPatientDocs([]); return; }
    setLoadingDocs(true);
    getDocuments(patientId)
      .then((data) => setPatientDocs([...data].sort((a, b) => b.createdAt.localeCompare(a.createdAt))))
      .catch(() => {})
      .finally(() => setLoadingDocs(false));
  }, [patientId]);

  const selectedPatient = patients.find((p) => p.id === patientId);

  const submit = async (e) => {
    e.preventDefault();
    if (!patientId) {
      alert("Please choose a patient.");
      return;
    }
    if (!reportText.trim() && !file) {
      alert("Write a report, attach a file, or both.");
      return;
    }

    setSubmitting(true);
    try {
      if (file) {
        const dataBase64 = await fileToBase64(file);
        await uploadDocument({
          patientId,
          patientName: selectedPatient?.name || "",
          uploaderRole: "Nurse",
          uploaderName: user?.name || "Nurse",
          filename: file.name,
          contentType: file.type || "application/octet-stream",
          dataBase64,
          note: reportText.trim(),
        });
      } else {
        const filename = `Nurse Report - ${new Date().toISOString().slice(0, 10)}.txt`;
        await uploadDocument({
          patientId,
          patientName: selectedPatient?.name || "",
          uploaderRole: "Nurse",
          uploaderName: user?.name || "Nurse",
          filename,
          contentType: "text/plain",
          dataBase64: textToBase64(reportText.trim()),
        });
      }

      alert("Report added. The patient (and their doctor) can now see it.");
      setReportText("");
      setFile(null);
      getDocuments(patientId)
        .then((data) => setPatientDocs([...data].sort((a, b) => b.createdAt.localeCompare(a.createdAt))))
        .catch(() => {});
    } catch (err) {
      alert("Could not add report: " + err.message);
    } finally {
      setSubmitting(false);
    }
  };

  return (
    <NurseLayout
      active="nurse-reports"
      navigate={navigate}
      logout={logout}
    >

      <div className="page-heading">

        <h1>Patient Reports</h1>

        <p>Add a care report for a patient. It becomes visible to the patient and their doctor immediately.</p>

      </div>

      <div className="form-layout">

        <form className="panel patient-form" onSubmit={submit}>

          <label>Patient *</label>
          <select value={patientId} onChange={(e) => setPatientId(e.target.value)}>
            <option value="">Select a patient</option>
            {patients.map((p) => (
              <option key={p.id} value={p.id}>{p.id} — {p.name}</option>
            ))}
          </select>

          <label>Report</label>
          <textarea
            placeholder="Vitals, observations, care notes..."
            value={reportText}
            onChange={(e) => setReportText(e.target.value)}
          />

          <label>Attach a file (optional)</label>
          <input
            type="file"
            accept=".txt,.doc,.docx,.pdf"
            onChange={(e) => setFile(e.target.files?.[0] || null)}
          />

          <button className="primary-button" type="submit" disabled={submitting}>
            {submitting ? "Adding..." : "Add Report"}
          </button>

        </form>

        <div className="panel">

          <h3>{selectedPatient ? `${selectedPatient.name}'s Reports` : "Select a patient"}</h3>

          {!patientId && <p className="dashboard-subtitle">Choose a patient to see their existing reports.</p>}

          {patientId && loadingDocs && <p className="dashboard-subtitle">Loading...</p>}

          {patientId && !loadingDocs && patientDocs.length === 0 && (
            <p className="dashboard-subtitle">No reports for this patient yet.</p>
          )}

          {patientDocs.map((doc) => (
            <div className="report-item" key={doc.id}>
              <span>📄</span>
              <div>
                <b>{doc.filename}</b>
                <small>
                  {doc.uploaderRole === "Nurse" ? `From ${doc.uploaderName}` : "From patient"} · {new Date(doc.createdAt).toLocaleDateString()}
                </small>
              </div>
              <a href={documentContentUrl(doc.id)} target="_blank" rel="noreferrer">
                <button>View</button>
              </a>
            </div>
          ))}

        </div>

      </div>

    </NurseLayout>
  );
}

export default Reports;
