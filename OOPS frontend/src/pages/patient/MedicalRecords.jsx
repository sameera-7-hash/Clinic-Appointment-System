import React, { useEffect, useRef, useState } from "react";
import PatientLayout from "./PatientLayout";
import { useCurrentUser } from "../../context/UserContext";
import { getDocuments, uploadDocument, fileToBase64, documentContentUrl } from "../../Services/api";

const ACCEPTED_EXTENSIONS = [".txt", ".doc", ".docx", ".pdf"];

function iconFor(filename) {
  const ext = filename.split(".").pop().toLowerCase();
  if (ext === "pdf") return "📕";
  if (ext === "doc" || ext === "docx") return "📘";
  return "📄";
}

function isAccepted(file) {
  const lower = file.name.toLowerCase();
  return ACCEPTED_EXTENSIONS.some((ext) => lower.endsWith(ext));
}

function MedicalRecords({ navigate, logout }) {

  const { user } = useCurrentUser();
  const [documents, setDocuments] = useState([]);
  const [loading, setLoading] = useState(true);
  const [uploading, setUploading] = useState(false);
  const [dragOver, setDragOver] = useState(false);
  const fileInputRef = useRef(null);

  const loadDocuments = () => {
    if (!user) return;
    setLoading(true);
    getDocuments(user.id)
      .then((data) => setDocuments([...data].sort((a, b) => b.createdAt.localeCompare(a.createdAt))))
      .catch(() => {})
      .finally(() => setLoading(false));
  };

  useEffect(loadDocuments, [user]);

  const uploadFiles = async (fileList) => {
    if (!user) return;
    const files = Array.from(fileList).filter(isAccepted);
    if (files.length === 0) {
      alert("Only .txt, .doc, .docx and .pdf files are accepted.");
      return;
    }

    setUploading(true);
    try {
      for (const file of files) {
        const dataBase64 = await fileToBase64(file);
        await uploadDocument({
          patientId: user.id,
          patientName: user.name,
          uploaderRole: "Patient",
          uploaderName: user.name,
          filename: file.name,
          contentType: file.type || "application/octet-stream",
          dataBase64,
        });
      }
      loadDocuments();
    } catch (err) {
      alert("Could not upload file: " + err.message);
    } finally {
      setUploading(false);
    }
  };

  const onDrop = (e) => {
    e.preventDefault();
    setDragOver(false);
    if (e.dataTransfer.files?.length) uploadFiles(e.dataTransfer.files);
  };

  return (
    <PatientLayout
      title="My Reports"
      active="medical-records"
      navigate={navigate}
      logout={logout}
    >

      <div className="patient-page-heading">

        <div>

          <span className="patient-eyebrow">
            MEDICAL RECORDS
          </span>

          <h1>My Reports</h1>

          <p>
            Upload your own documents, and see reports your care team has shared with you.
          </p>

        </div>

      </div>

      <div
        className={`dropzone${dragOver ? " dragover" : ""}`}
        onDragOver={(e) => { e.preventDefault(); setDragOver(true); }}
        onDragLeave={() => setDragOver(false)}
        onDrop={onDrop}
        onClick={() => fileInputRef.current?.click()}
      >
        <input
          ref={fileInputRef}
          type="file"
          multiple
          accept={ACCEPTED_EXTENSIONS.join(",")}
          style={{ display: "none" }}
          onChange={(e) => { if (e.target.files?.length) uploadFiles(e.target.files); e.target.value = ""; }}
        />
        <div className="dropzone-icon">📤</div>
        <p><strong>Drag & drop</strong> a file here, or click to browse.</p>
        <small>Accepted: .txt, .doc, .docx, .pdf</small>
        {uploading && <p className="dashboard-subtitle">Uploading...</p>}
      </div>


      <section className="reports-modern-list">

        {!loading && documents.length === 0 && (
          <p className="dashboard-subtitle">No reports yet. Upload one above, or check back after your next visit.</p>
        )}

        {documents.map((doc) => (

          <div
            className="report-modern-card"
            key={doc.id}
          >

            <div
              className={`report-icon ${doc.uploaderRole === "Nurse" ? "green" : "blue"}`}
            >
              {iconFor(doc.filename)}
            </div>


            <div className="report-main-info">

              <h3>{doc.filename}</h3>

              <p>
                {doc.uploaderRole === "Nurse" ? `From ${doc.uploaderName || "your nurse"}` : "Uploaded by you"}
                <span>•</span>
                {new Date(doc.createdAt).toLocaleDateString()}
              </p>

              {doc.note && <p className="report-note">{doc.note}</p>}

            </div>


            <a href={documentContentUrl(doc.id)} target="_blank" rel="noreferrer">
              <button className="report-view-btn">
                View Report
              </button>
            </a>

          </div>

        ))}

      </section>

    </PatientLayout>
  );
}

export default MedicalRecords;
