VENV ?= .venv
PY := $(VENV)/bin/python
PIP := $(VENV)/bin/pip

.PHONY: venv install run test lint fmt

venv:
	@test -x "$(PY)" || python3 -m venv $(VENV)
	@$(PY) -m pip install -U pip

install: venv
	$(PY) -m pip install -r requirements.txt

run: venv
	$(PY) -m uvicorn netdiag_api.app:app --host 127.0.0.1 --port $${PORT:-8080}

test: venv
	$(PY) -m pytest -q

lint: venv
	$(PY) -m ruff check .
	$(PY) -m black --check .
	$(PY) -m bandit -r netdiag_api -c bandit.yaml

fmt: venv
	$(PY) -m ruff check . --fix
	$(PY) -m black .