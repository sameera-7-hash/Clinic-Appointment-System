import React, { useEffect, useState } from "react";

function readStoredTheme() {
  try {
    return localStorage.getItem("pc_theme") || "light";
  } catch {
    return "light";
  }
}

function applyTheme(theme) {
  document.documentElement.setAttribute("data-theme", theme);
  try {
    localStorage.setItem("pc_theme", theme);
  } catch {
    // Private browsing / blocked storage -- theme just won't persist.
  }
}

// Light/dark toggle for the top bar. Applies data-theme to <html> so it
// takes effect regardless of which page/layout is currently mounted.
function ThemeToggle() {
  const [theme, setTheme] = useState(readStoredTheme);

  useEffect(() => {
    applyTheme(theme);
  }, []);

  const toggle = () => {
    const next = theme === "dark" ? "light" : "dark";
    setTheme(next);
    applyTheme(next);
  };

  return (
    <button
      type="button"
      className="icon-btn theme-toggle"
      onClick={toggle}
      title={theme === "dark" ? "Switch to light mode" : "Switch to dark mode"}
    >
      {theme === "dark" ? "☀️" : "🌙"}
    </button>
  );
}

export default ThemeToggle;
