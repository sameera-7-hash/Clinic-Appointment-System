import { defineConfig } from "vite";

// Proxies /api/* to the C++ patient_server (see patient_dashboard/backend,
// run with `patient_server` listening on :8080) so the frontend can call
// same-origin relative paths and avoid dealing with CORS in dev.
export default defineConfig({
  server: {
    proxy: {
      "/api": {
        target: "http://localhost:8080",
        changeOrigin: true,
      },
    },
  },
});
