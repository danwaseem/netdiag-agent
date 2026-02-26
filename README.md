# netdiag-agent

A small network diagnostics agent written in **C**, plus a lightweight **Python (FastAPI)** API wrapper that exposes the collected metrics over HTTP.

This project demonstrates:
* **Linux-style system introspection** (/proc, /sys)
* **Raw networking** (ICMP echo on Linux)
* **Safe fallback** to system ping
* **Atomic file writes** in C
* **REST API integration** with FastAPI
* **Unit testing** (C + Python)
* **Linting & formatting** (clang-format, ruff, black)
* **Security scanning** (bandit)
* **CI/CD integration** (GitHub Actions / docker script)

---

## 📦 Repository Layout

```text
netdiag-agent/
├── agent_c/                # C agent source and Makefile
├── agent_py/               # FastAPI API service
├── scripts/                # Helper scripts (CI/local)
├── .github/workflows/      # CI configuration
├── README.md
└── .gitignore
```

---

## 🚀 What It Does

### 1. C Agent (agent_c/)
Builds a binary called `netdiag_agent`. The agent performs the following:

* **System Stats:** Reads uptime from `/proc/uptime` and parses interface statistics (RX/TX bytes and errors) from `/proc/net/dev`.
* **Connectivity:** Performs checks using **raw ICMP** on Linux (when permitted) or falls back to executing `/bin/ping`.
* **Metrics:** Computes average latency (ms) and packet loss (%).
* **Output:** Generates a JSON payload to `stdout` and `/tmp/netdiag.json` using an **atomic write** (temp file + rename) to prevent race conditions with the API.

### 2. Python API (agent_py/)
A FastAPI server that monitors the metrics file and exposes the following endpoints:

| Endpoint | Description |
| :--- | :--- |
| GET / | Basic API info |
| GET /health | API health + heartbeat timestamp |
| GET /metrics | Returns the latest metrics JSON |

**Status Codes:**
* **200**: Valid JSON returned.
* **503**: Metrics file not found (Agent hasn't run).
* **500**: Invalid JSON/Internal error.

---

## 🔧 Build & Run

### C Agent
```bash
cd agent_c
make fmt
make clean build
make test_c
make run
```

### Python API
```bash
cd agent_py
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -U pip
make install
make lint
make test
make run
```

---

## 🧪 CI/CD & Security

### Local CI Emulation
Run the full suite of checks locally using the provided Docker script:
```bash
./scripts/docker_ci.sh
```

### Security Practices
* **No system() calls:** Uses safe `execv()` for pings to prevent command injection.
* **Atomic Operations:** Writes to a .tmp file and renames it to ensure the API never reads a partial file.
* **Static Analysis:** Bandit scans Python code; strict compiler flags (-Wall -Wextra -Werror) for C.

---

## 📊 Example Output

```json
{
  "timestamp": "2026-02-23T12:10:00Z",
  "uptime": 14520,
  "ping": {
    "avg_ms": 9.83,
    "loss_pct": 0.0,
    "sent": 4,
    "received": 4
  },
  "interfaces": [
    {
      "name": "eth0",
      "rx_bytes": 1849201,
      "tx_bytes": 1920391,
      "rx_errs": 0,
      "tx_errs": 0
    }
  ]
}
```
