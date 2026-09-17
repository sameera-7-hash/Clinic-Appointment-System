# Patient Records Dashboard (C++ backend + web frontend)

A small full-stack app: a **C++ REST API** stores patient records **encrypted at rest**
(your choice of AES-256 or DES per record), appends them to a **tamper-evident,
hash-chained** file with an automatic **backup mirror**, and a plain **HTML/JS
dashboard** talks to it over HTTP.

## How the pieces fit together

```
Browser (frontend/index.html)
   |  fetch() calls to /api/...
   v
C++ HTTP server (backend/src/main.cpp, cpp-httplib)
   |
   v
PatientStorage (backend/src/Storage.cpp)
   |  encrypts via a CipherStrategy (AesCipher or DesCipher)
   v
data/patients.dat   (primary, encrypted, hash-chained)
data/patients.dat.bak (mirror, updated on every write)
```

This *is* "using C++ as a backend": the C++ program is the HTTP server the
browser talks to, not a script called from something else. The frontend
never touches encryption or files directly — it only calls the REST API.

### Classes (OOP structure)

- **`Patient`** (`include/Patient.h`) — plain data class for one record.
- **`CipherStrategy`** (`include/Cipher.h`) — abstract interface with two
  implementations, **`AesCipher`** and **`DesCipher`** (Strategy pattern).
  Storage code only ever talks to `CipherStrategy`, so a new algorithm can
  be added later without touching `PatientStorage` or the API.
- **`PatientStorage`** (`include/Storage.h` / `src/Storage.cpp`) — owns
  reading/writing the encrypted file, the hash chain, and backup recovery.
- **`main.cpp`** — wires HTTP routes to the classes above. No business
  logic lives here.

## Encryption: AES vs DES

- **AES-256-CBC** is the one that should actually protect real data.
- **DES-CBC** is included because it was asked for, but it is genuinely
  broken by modern standards — a 56-bit key can be brute-forced in hours
  on rented cloud hardware. It's wired up so you can pick either cipher
  per patient in the dashboard and see both the code path and the
  resulting ciphertext side by side. **Don't use DES for data you care
  about protecting; use AES.**
- The key used for both is derived from a passphrase (`PATIENT_DB_KEY`,
  see below) via SHA-256. That's a simple derivation, not a hardened one
  — a production system would use PBKDF2/scrypt/Argon2 with a per-record
  salt instead. Said plainly here so it isn't mistaken for one.
- Each encryption uses a fresh random IV, stored alongside the ciphertext.

## Storage, "keep appending", and integrity/backup

- Records are **appended**, never rewritten — `data/patients.dat` only
  grows, one JSON line per patient, matching "store patients and keep
  adding to it over time."
- Each line also carries a **`chain_hash`**: `SHA256(previous line's
  chain_hash + this line's encrypted bytes)`. That links every record to
  the one before it — like a minimal ledger — so editing, deleting, or
  reordering *any* past record is detectable later, not just the most
  recent one. The dashboard shows "Integrity chain OK / BROKEN" using
  this.
- Every successful write is mirrored to **`data/patients.dat.bak`**
  immediately. If the primary file is ever missing, truncated, or has
  fewer valid records than the backup (crash mid-write, disk error,
  accidental edit), the server automatically reads from the backup
  instead and reports `usedBackup: true`.
- The "Snapshot backup" button in the dashboard also makes an explicit,
  timestamped full copy (`patients.dat.<timestamp>.snapshot`) on demand.

This was tested directly (not just assumed to work): a tampered record is
correctly flagged while unrelated records before/after it stay marked
`ok: true`, and a primary file truncated to empty is correctly recovered
from the backup file.

## Building and running

Requirements: a C++17 compiler, CMake, and OpenSSL dev headers (already
present on most Linux dev boxes; on Ubuntu/Debian: `apt install
build-essential cmake libssl-dev`).

```bash
cd backend
mkdir -p build && cd build
cmake ..
make
cd ..                      # IMPORTANT: run the server from backend/,
                            # not from backend/build/ — it serves the
                            # frontend from a path relative to its cwd.
PATIENT_DB_KEY="choose-a-real-passphrase" ./build/patient_server
```

Then open **http://localhost:8080** in a browser — that's the dashboard,
served by the same C++ process (no separate frontend server needed).

If you skip `PATIENT_DB_KEY`, the server still runs (for convenience) but
prints a warning and uses a demo passphrase — don't do that with real data.

### API endpoints, if you want to script against it directly

| Method | Path | Body | Purpose |
|---|---|---|---|
| GET | `/api/patients` | — | list all patients, decrypted, with integrity status |
| POST | `/api/patients` | `{name, age, gender, contact, diagnosis, admissionDate, algo: "AES"\|"DES"}` | add a patient |
| POST | `/api/backup` | — | force a timestamped snapshot |
| GET | `/api/health` | — | liveness check |

## Known limitations / next steps

- Key derivation is SHA-256, not PBKDF2/Argon2 — fine for a prototype,
  not for production secrets handling.
- No authentication/authorization on the API yet — anyone who can reach
  port 8080 can read and add records. Add an auth layer before exposing
  this beyond localhost.
- No per-user roles (e.g. doctor vs admin) yet — everyone who can reach
  the dashboard has full access.
- IDs are assigned from the current record count, so they're stable only
  as long as no record is deleted (there's no delete endpoint yet, by
  design — this is an append-only ledger).
