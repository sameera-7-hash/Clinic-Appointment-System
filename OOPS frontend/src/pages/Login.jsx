import React, { useState } from "react";
import { login as loginApi, signup as signupApi } from "../Services/api";
import Reveal from "../components/Reveal";
import { Tabs, TabsList, TabsTrigger } from "../components/ui/Tabs";

function Login({ role, onAuthenticated, onBack }) {

  const [mode, setMode] = useState("login"); // "login" | "signup"

  const [name, setName] = useState("");
  const [email, setEmail] = useState("");
  const [password, setPassword] = useState("");
  const [showPassword, setShowPassword] = useState(false);
  const [submitting, setSubmitting] = useState(false);
  const [error, setError] = useState("");

  const handleSubmit = async (e) => {
    e.preventDefault();
    setError("");

    if (!email || !password || (mode === "signup" && !name)) {
      setError("Please fill in all fields.");
      return;
    }

    setSubmitting(true);
    try {
      const user =
        mode === "signup"
          ? await signupApi({ name, email, password, role })
          : await loginApi({ email, password });

      if (mode === "login" && user.role !== role) {
        setError(`This account is registered as ${user.role}, not ${role}.`);
        return;
      }

      onAuthenticated(user);
    } catch (err) {
      setError(err.message);
    } finally {
      setSubmitting(false);
    }
  };


  const getRoleIcon = () => {

    if (role === "Patient") return "👤";

    if (role === "Doctor") return "⚕";

    return "✚";
  };


  const getDescription = () => {

    if (role === "Patient") {

      return "Access your health records, appointments and prescriptions.";
    }

    if (role === "Doctor") {

      return "Manage patients, consultations, prescriptions and reports.";
    }

    return "Manage patient care, tasks, appointments and alerts.";
  };


  return (

    <div className="login-page">


      <div className="login-left">

        <div className="login-top">

          <div className="login-brand">

            <div className="login-logo">
              ✚
            </div>

            <div>
              <h2>PatientCare</h2>
              <p>Secure Healthcare Management</p>
            </div>

          </div>


          <button
            className="login-back-button"
            onClick={onBack}
          >
            ← Back to Home
          </button>

        </div>


        <Reveal as="div" className="login-form-container">

          <div className="role-login-icon">
            {getRoleIcon()}
          </div>


          <h1>
            {role} {mode === "signup" ? "Sign Up" : "Login"}
          </h1>


          <p className="login-welcome">
            {mode === "signup" ? "Create your account to get started." : "Welcome back! Please login to your account."}
          </p>


          <p className="login-description">
            {getDescription()}
          </p>

          <Tabs
            value={mode}
            onValueChange={(next) => { setMode(next); setError(""); }}
            className="mb-4 mt-3.5"
          >
            <TabsList>
              <TabsTrigger value="login">Login</TabsTrigger>
              <TabsTrigger value="signup">Create Account</TabsTrigger>
            </TabsList>
          </Tabs>


          <form onSubmit={handleSubmit}>

            {mode === "signup" && (
              <div className="input-group">

                <label>
                  Full Name
                </label>

                <div className="input-wrapper">

                  <span>
                    🧑
                  </span>

                  <input
                    type="text"
                    placeholder="Enter your full name"
                    value={name}
                    onChange={(e) => setName(e.target.value)}
                  />

                </div>

              </div>
            )}

            <div className="input-group">

              <label>
                Email
              </label>

              <div className="input-wrapper">

                <span>
                  👤
                </span>

                <input
                  type="email"
                  placeholder={
                    role === "Patient"
                      ? "Enter your patient email"
                      : role === "Doctor"
                      ? "Enter your doctor email"
                      : "Enter your nurse email"
                  }
                  value={email}
                  onChange={(e) =>
                    setEmail(e.target.value)
                  }
                />

              </div>

            </div>


            <div className="input-group">

              <label>
                Password
              </label>

              <div className="input-wrapper">

                <span>
                  🔒
                </span>

                <input
                  type={
                    showPassword
                      ? "text"
                      : "password"
                  }
                  placeholder="Enter your password"
                  value={password}
                  onChange={(e) =>
                    setPassword(e.target.value)
                  }
                />

                <button
                  type="button"
                  className="show-password"
                  onClick={() =>
                    setShowPassword(!showPassword)
                  }
                >
                  {showPassword
                    ? "Hide"
                    : "Show"}
                </button>

              </div>

            </div>

            {error && <p className="auth-error">{error}</p>}

            <button
              type="submit"
              className="main-login-button"
              disabled={submitting}
            >
              {submitting
                ? "Please wait..."
                : mode === "signup"
                ? `Create ${role} Account →`
                : `Login as ${role} →`}
            </button>

          </form>


          <div className="login-security">

            <div>
              🔐

              <span>
                <strong>Secure</strong>
                Protected login
              </span>

            </div>


            <div>
              🛡

              <span>
                <strong>Encrypted</strong>
                Data protection
              </span>

            </div>


            <div>
              ✓

              <span>
                <strong>Trusted</strong>
                Secure access
              </span>

            </div>

          </div>

        </Reveal>

      </div>


      <div className="login-right">

        <div className="login-illustration">

          <div className="doctor-circle">

            {role === "Patient"
              ? "🧑‍💻"
              : "👩‍⚕️"}

          </div>


          <div className="login-floating-card secure-card">

            🔐

            <div>
              <strong>Secure</strong>
              <small>Your account is protected</small>
            </div>

          </div>


          <div className="login-floating-card encrypted-card">

            🛡️

            <div>
              <strong>Encrypted</strong>
              <small>Your health data is protected</small>
            </div>

          </div>


          <div className="login-floating-card trusted-card">

            ✓

            <div>
              <strong>Trusted</strong>
              <small>Reliable healthcare platform</small>
            </div>

          </div>

        </div>


        <div className="login-right-text">

          <h2>
            Your Health.
            <br />
            Our Priority.
          </h2>

          <p>
            PatientCare provides secure access to your
            healthcare information anytime, anywhere.
          </p>

        </div>

      </div>

    </div>
  );
}

export default Login;
