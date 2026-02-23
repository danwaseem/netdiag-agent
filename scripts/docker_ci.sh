#!/usr/bin/env bash
set -euo pipefail
docker run --rm -it --platform linux/arm64 -v "$(pwd)":/work -w /work ubuntu:22.04 bash -lc '
  set -e
  apt-get update
  apt-get install -y build-essential clang-format python3 python3-pip iputils-ping
  cd agent_c
  make build
  make test_c
  make lint
  make forbid_system
  cd ../agent_py
  python3 -m pip install -r requirements.txt
  pytest -q
  ruff check .
  black --check .
  bandit -r . -c bandit.yaml
'
