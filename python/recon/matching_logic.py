"""High-level matching logic bridging Python CLI to SQLite store."""
from __future__ import annotations

import sqlite3
from pathlib import Path
from typing import Any


def run_matching(cfg: dict[str, Any], dry_run: bool = False) -> dict[str, Any]:
    """Run full matching pipeline against SQLite database."""
    db_path = cfg["database"]["path"]
    Path(db_path).parent.mkdir(parents=True, exist_ok=True)

    conn = sqlite3.connect(db_path)
    conn.row_factory = sqlite3.Row
    _init_schema(conn)

    internals = [dict(r) for r in conn.execute("SELECT * FROM internal_trades").fetchall()]
    confirms = [dict(r) for r in conn.execute("SELECT * FROM confirmations").fetchall()]

    tol_price = cfg["matching"]["price_tolerance"]
    tol_qty = cfg["matching"]["qty_tolerance"]

    confirm_map: dict[str, list[dict]] = {}
    for c in confirms:
        confirm_map.setdefault(c["trade_id"], []).append(c)

    matched = 0
    qty_mismatch = 0
    price_mismatch = 0
    missing_broker = 0
    missing_internal = 0
    exceptions: list[dict[str, Any]] = []
    matched_tids: set[str] = set()

    for t in internals:
        tid = t["trade_id"]
        if tid in confirm_map:
            c = confirm_map[tid][0]
            matched_tids.add(tid)
            qd = abs(t["quantity"] - c["quantity"])
            pd = abs(t["price"] - c["price"])
            if qd <= tol_qty and pd <= tol_price:
                matched += 1
            elif qd > tol_qty:
                qty_mismatch += 1
                exceptions.append({
                    "trade_id": tid, "type": "qty_mismatch",
                    "description": f"Qty: {t['quantity']} vs {c['quantity']}",
                })
            else:
                price_mismatch += 1
                exceptions.append({
                    "trade_id": tid, "type": "price_mismatch",
                    "description": f"Price: {t['price']} vs {c['price']}",
                })
        else:
            missing_broker += 1
            exceptions.append({"trade_id": tid, "type": "missing_broker", "description": "No broker confirmation"})

    for c in confirms:
        if c["trade_id"] not in matched_tids:
            missing_internal += 1
            exceptions.append({
                "trade_id": c["trade_id"], "type": "missing_internal",
                "description": f"No internal trade for {c['confirm_id']}",
            })

    # Persist exceptions
    if not dry_run and exceptions:
        for ex in exceptions:
            conn.execute(
                "INSERT INTO exceptions (match_state, severity, state, internal_trade_id, symbol, description) "
                "VALUES (?, ?, 'Detected', ?, '', ?)",
                (ex["type"], "High", ex["trade_id"], ex.get("description", "")),
            )
        conn.commit()

    conn.close()

    return {
        "total_internal": len(internals),
        "total_confirmations": len(confirms),
        "matched": matched,
        "qty_mismatch": qty_mismatch,
        "price_mismatch": price_mismatch,
        "missing_broker": missing_broker,
        "missing_internal": missing_internal,
        "exceptions": exceptions,
        "dry_run": dry_run,
    }


def _init_schema(conn: sqlite3.Connection) -> None:
    conn.executescript("""
        CREATE TABLE IF NOT EXISTS internal_trades (
            trade_id TEXT NOT NULL,
            timestamp_ns INTEGER NOT NULL,
            symbol TEXT NOT NULL,
            side INTEGER NOT NULL,
            quantity INTEGER NOT NULL,
            price REAL NOT NULL,
            venue TEXT NOT NULL,
            strategy TEXT DEFAULT '',
            account TEXT DEFAULT '',
            status TEXT DEFAULT 'active',
            PRIMARY KEY (trade_id, venue)
        );
        CREATE TABLE IF NOT EXISTS confirmations (
            confirm_id TEXT NOT NULL,
            trade_id TEXT NOT NULL,
            symbol TEXT NOT NULL,
            side INTEGER NOT NULL,
            quantity INTEGER NOT NULL,
            price REAL NOT NULL,
            venue TEXT NOT NULL,
            status TEXT DEFAULT 'confirmed',
            fees REAL DEFAULT 0.0,
            timestamp_ns INTEGER NOT NULL,
            PRIMARY KEY (confirm_id, venue)
        );
        CREATE TABLE IF NOT EXISTS exceptions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            match_state TEXT,
            severity TEXT,
            state TEXT DEFAULT 'Detected',
            internal_trade_id TEXT,
            broker_trade_id TEXT,
            symbol TEXT,
            description TEXT,
            created_at_ns INTEGER,
            updated_at_ns INTEGER,
            assignee TEXT DEFAULT '',
            notes TEXT DEFAULT '',
            resolution TEXT DEFAULT ''
        );
    """)
