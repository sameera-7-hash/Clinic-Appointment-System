import React from "react";
import Reveal from "../components/Reveal";

const FEATURES = [
  {
    icon: "🔐",
    title: "AES-256 & DES Encryption",
    text: "Every patient record is encrypted at rest, with a legacy cipher included side-by-side for a security comparison.",
  },
  {
    icon: "🔗",
    title: "Tamper-Evident Hash Chain",
    text: "Each record links cryptographically to the one before it — an edit anywhere in history is instantly detectable.",
  },
  {
    icon: "👥",
    title: "Three Role-Based Portals",
    text: "Purpose-built dashboards for doctors, nurses and patients, each seeing exactly what they need — and nothing else.",
  },
  {
    icon: "📅",
    title: "Live Appointment Booking",
    text: "Doctors publish real availability; patients book real open slots — no double-booking, ever.",
  },
  {
    icon: "📤",
    title: "Secure Document Sharing",
    text: "Patients upload reports, nurses attach care notes — instantly visible to the right people, and no one else.",
  },
  {
    icon: "🔔",
    title: "Real-Time Reminders",
    text: "Booking an appointment notifies the patient, the doctor and nursing staff the moment it's confirmed.",
  },
];

function Home({ onLogin }) {
  return (
    <div className="home-page">

      <header className="home-header">

        <div className="home-brand">

          <div className="home-logo">
            ✚
          </div>

          <div>
            <h2>PatientCare</h2>
            <p>Secure Healthcare Management</p>
          </div>

        </div>

        <nav className="home-nav">

          <a href="#about">
            About Project
          </a>

          <a href="#features">
            Features
          </a>

          <button
            className="home-login-btn"
            onClick={onLogin}
          >
            Login
          </button>

        </nav>

      </header>


      <main className="home-main">

        <section className="home-content">

          <Reveal className="home-text" as="div">

            <div className="home-badge">
              🔒 Secure Healthcare Platform
            </div>

            <h1>
              Welcome To <span>PatientCare</span>
            </h1>

            <h2>
              Your Health, Your Priority
            </h2>

            <p>
              A secure and reliable healthcare management platform
              designed to connect patients, doctors and nurses in one
              place.
            </p>

            <div className="home-buttons">

              <button
                className="primary-home-btn"
                onClick={onLogin}
              >
                Login to PatientCare →
              </button>

              <a className="secondary-home-btn" href="#about">
                Learn More
              </a>

            </div>

          </Reveal>


          <Reveal className="home-illustration" delay={150}>

            <div className="illustration-circle">
              👩‍⚕️
            </div>

            <div className="floating-card card-one">

              🔐

              <div>
                <strong>Secure</strong>
                <small>Your data is protected</small>
              </div>

            </div>


            <div className="floating-card card-two">

              🛡️

              <div>
                <strong>Encrypted</strong>
                <small>Protected health records</small>
              </div>

            </div>


            <div className="floating-card card-three">

              ✓

              <div>
                <strong>Trusted</strong>
                <small>Reliable healthcare</small>
              </div>

            </div>

          </Reveal>

        </section>


        <section className="home-about" id="about">

          <Reveal as="div" className="home-about-text">
            <span className="home-eyebrow">ABOUT THIS PROJECT</span>
            <h2>A Full-Stack Clinic System, Built to Demonstrate Real OOP Design</h2>
            <p>
              PatientCare pairs a React frontend with a C++ REST API. The
              backend uses the Strategy pattern to swap encryption
              algorithms per record, keeps patient data in an append-only,
              hash-chained store so tampering anywhere in history is
              detectable, and exposes accounts, appointments, document
              sharing and live reminders — all wired end-to-end into the
              dashboards below.
            </p>
          </Reveal>

          <div className="home-stats-row">
            <Reveal delay={0} className="home-stat"><strong>3</strong><span>Role-based portals</span></Reveal>
            <Reveal delay={80} className="home-stat"><strong>2</strong><span>Encryption algorithms</span></Reveal>
            <Reveal delay={160} className="home-stat"><strong>C++</strong><span>REST backend</span></Reveal>
            <Reveal delay={240} className="home-stat"><strong>0</strong><span>Double-booked slots</span></Reveal>
          </div>

        </section>


        <section className="home-features" id="features">

          <Reveal as="h2" className="home-features-heading">
            What Powers PatientCare
          </Reveal>

          <div className="home-features-grid">
            {FEATURES.map((f, i) => (
              <Reveal key={f.title} delay={i * 90} className="home-feature-card">
                <div className="home-feature-icon">{f.icon}</div>
                <h3>{f.title}</h3>
                <p>{f.text}</p>
              </Reveal>
            ))}
          </div>

        </section>


        <section className="home-security">

          <div className="home-security-item">

            <span>🔐</span>

            <div>
              <strong>Secure</strong>
              <p>Your information is protected</p>
            </div>

          </div>


          <div className="home-security-item">

            <span>🛡️</span>

            <div>
              <strong>Encrypted</strong>
              <p>Healthcare data stays protected</p>
            </div>

          </div>


          <div className="home-security-item">

            <span>✓</span>

            <div>
              <strong>Trusted</strong>
              <p>Designed for secure healthcare</p>
            </div>

          </div>

        </section>

      </main>

    </div>
  );
}

export default Home;
