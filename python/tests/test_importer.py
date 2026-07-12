from __future__ import annotations

import csv
import json
import tempfile
from pathlib import Path

from recon.importer import import_csv, import_json, import_fix


def _cfg(tmp_path: Path) -> dict:
    return {"database": {"path": str(tmp_path / "test.db")}}


def test_import_csv_internal(tmp_path):
    csv_path = tmp_path / "trades.csv"
    with open(csv_path, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["trade_id", "timestamp_ns", "symbol", "side", "quantity", "price", "venue", "strategy", "account", "status"])
        writer.writerow(["T001", "1700000000000000000", "AAPL", "Buy", "100", "150.0", "NYSE", "stat_arb", "ACC01", "active"])
        writer.writerow(["T002", "1700000001000000000", "GOOG", "Sell", "50", "2800.0", "NASDAQ", "momentum", "ACC02", "active"])

    count = import_csv(str(csv_path), "internal", _cfg(tmp_path))
    assert count == 2


def test_import_csv_confirmation(tmp_path):
    csv_path = tmp_path / "confirms.csv"
    with open(csv_path, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["confirm_id", "trade_id", "symbol", "side", "quantity", "price", "venue", "status", "fees", "timestamp_ns"])
        writer.writerow(["C001", "T001", "AAPL", "Buy", "100", "150.0", "NYSE", "confirmed", "1.5", "1700000000000000000"])

    count = import_csv(str(csv_path), "confirmation", _cfg(tmp_path))
    assert count == 1


def test_import_csv_malformed(tmp_path):
    csv_path = tmp_path / "bad.csv"
    with open(csv_path, "w") as f:
        f.write("invalid,data,here\nno_columns_match\n")

    count = import_csv(str(csv_path), "internal", _cfg(tmp_path))
    assert count == 0


def test_import_json_internal(tmp_path):
    json_path = tmp_path / "trades.json"
    data = [
        {"trade_id": "T001", "timestamp_ns": 1700000000000000000, "symbol": "AAPL", "side": "Buy", "quantity": 100, "price": 150.0, "venue": "NYSE"},
        {"trade_id": "T002", "timestamp_ns": 1700000001000000000, "symbol": "MSFT", "side": "Sell", "quantity": 200, "price": 380.0, "venue": "NASDAQ"},
    ]
    json_path.write_text(json.dumps(data))

    count = import_json(str(json_path), "internal", _cfg(tmp_path))
    assert count == 2


def test_import_json_confirmation(tmp_path):
    json_path = tmp_path / "confirms.json"
    data = [{"confirm_id": "C001", "trade_id": "T001", "symbol": "AAPL", "side": "Buy", "quantity": 100, "price": 150.0, "venue": "NYSE"}]
    json_path.write_text(json.dumps(data))

    count = import_json(str(json_path), "confirmation", _cfg(tmp_path))
    assert count == 1


def test_import_fix_messages(tmp_path):
    fix_path = tmp_path / "fix.txt"
    soh = "\x01"
    msg1 = f"8=4.4{soh}9=50{soh}35=D{soh}11=ORD001{soh}55=AAPL{soh}54=1{soh}38=100{soh}44=150.50{soh}49=TRC{soh}10=123{soh}"
    fix_path.write_text(msg1 + "\n")

    count = import_fix(str(fix_path), "confirmation", _cfg(tmp_path))
    assert count == 1


def test_import_fix_empty(tmp_path):
    fix_path = tmp_path / "empty.txt"
    fix_path.write_text("")

    count = import_fix(str(fix_path), "confirmation", _cfg(tmp_path))
    assert count == 0


def test_import_json_wrong_fields(tmp_path):
    json_path = tmp_path / "bad.json"
    json_path.write_text('[{"foo": "bar"}]')

    count = import_json(str(json_path), "internal", _cfg(tmp_path))
    assert count == 0
