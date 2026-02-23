#!/usr/bin/env bash
set -euo pipefail
docker run --rm -it --platform linux/arm64 -v "$(pwd)":/work -p 8080:8080 -w /work ubuntu:22.04 bash -lc '
  set -e
  apt-get update
  apt-get install -y build-essential python3 python3-pip iputils-ping
  cd agent_c
  make build
  NETDIAG_INTERVAL=5 ./netdiag_agent &
  sleep 2
  cd ../agent_py
  python3 -m pip install -r requirements.txt
  python3 -m uvicorn netdiag_api.app:app --host 0.0.0.0 --port 8080
'
