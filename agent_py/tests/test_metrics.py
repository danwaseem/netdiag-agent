import json
from fastapi.testclient import TestClient
from netdiag_api.app import app

client = TestClient(app)


def test_missing(tmp_path, monkeypatch):
    monkeypatch.setenv("NETDIAG_JSON_PATH", str(tmp_path / "missing.json"))
    assert client.get("/metrics").status_code == 503


def test_ok(tmp_path, monkeypatch):
    p = tmp_path / "netdiag.json"
    p.write_text(json.dumps({"target": "8.8.8.8"}), encoding="utf-8")
    monkeypatch.setenv("NETDIAG_JSON_PATH", str(p))
    r = client.get("/metrics")
    assert r.status_code == 200
    assert r.json()["target"] == "8.8.8.8"


def test_invalid(tmp_path, monkeypatch):
    p = tmp_path / "netdiag.json"
    p.write_text("{nope", encoding="utf-8")
    monkeypatch.setenv("NETDIAG_JSON_PATH", str(p))
    assert client.get("/metrics").status_code == 500
