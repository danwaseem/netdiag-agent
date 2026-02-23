# netdiag-agent

A small network diagnostics agent written in C, plus a lightweight Python (FastAPI) API wrapper.

## Repository layout

- `agent_c/` — C agent binary (`netdiag_agent`)
- `agent_py/` — FastAPI service exposing `/health` and `/metrics`
- `scripts/` — helper scripts for CI / local runs
- `.github/workflows/` — CI workflows

## Requirements

### For C agent (local)

- clang or gcc
- make
- clang-format

### For Python API (local)

- Python 3.10+ (recommended)
- pip
- A virtual environment (strongly recommended)

---

## Build and test (C)

cd agent_c

# Format
make fmt

# Build and test
make clean
make build
make test_c

## Run (C)
cd agent_c
make run
Set up Python (venv + deps)
macOS often blocks system-wide pip installs (PEP 668). Use a virtual environment:


cd agent_py

## Create and activate venv (macOS / Linux)
python3 -m venv .venv
source .venv/bin/activate

## Upgrade pip and install dependencies
python -m pip install -U pip
make install


## Lint and test (Python)


cd agent_py
source .venv/bin/activate

make lint
make test

## Run (Python API)

cd agent_py
source .venv/bin/activate

make run
Service is available at:

http://127.0.0.1:8080

## CI (same checks as GitHub)

./scripts/docker_ci.sh

## Notes
On macOS, raw ICMP sockets typically require elevated privileges, and some network structs differ from Linux.

The C ping implementation falls back to executing the system ping binary when raw ICMP is unavailable.

Bandit configuration lives in agent_py/bandit.yaml.