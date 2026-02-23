import json
import os
from datetime import datetime
from pathlib import Path
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel

app = FastAPI(title="NetDiag API")


def json_path() -> str:
    return os.getenv("NETDIAG_JSON_PATH", "/tmp/netdiag.json")


class HealthResp(BaseModel):
    status: str
    timestamp: str


@app.get("/health", response_model=HealthResp)
def health() -> HealthResp:
    return HealthResp(status="ok", timestamp=datetime.utcnow().isoformat() + "Z")


@app.get("/metrics")
def metrics():
    p = Path(json_path())
    if not p.exists():
        raise HTTPException(status_code=503, detail=f"{p} not found")
    try:
        data = json.loads(p.read_text(encoding="utf-8"))
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"invalid json: {e}")
    if not isinstance(data, dict):
        raise HTTPException(status_code=500, detail="invalid json: expected object")
    return data
