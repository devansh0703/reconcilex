from __future__ import annotations

import csv
import json
import logging
import sqlite3
from pathlib import Path

logger = logging.getLogger(__name__)


def _get_conn(cfg: dict) -> sqlite3.Connection:
    db_path = cfg["database"]["path"]
    Path(db_path).parent.mkdir(parents=True, exist_ok=True)
    conn = sqlite3.connect(db_path)
    _init_schema(conn)
    return conn


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


def import_csv(filepath: str, rec_type: str, cfg: dict) -> int:
    """Import records from a CSV file."""
    conn = _get_conn(cfg)
    count = 0

    with open(filepath) as f:
        reader = csv.DictReader(f)
        for row in reader:
            try:
                if rec_type == "internal":
                    conn.execute(
                        "INSERT OR IGNORE INTO internal_trades "
                        "(trade_id, timestamp_ns, symbol, side, quantity, price, venue, strategy, account, status) "
                        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
                        (
                            row["trade_id"],
                            int(row.get("timestamp_ns", 0)),
                            row["symbol"],
                            1 if row.get("side", "Buy") == "Buy" else 0,
                            int(row.get("quantity", 0)),
                            float(row.get("price", 0)),
                            row.get("venue", "NYSE"),
                            row.get("strategy", ""),
                            row.get("account", ""),
                            row.get("status", "active"),
                        ),
                    )
                elif rec_type == "confirmation":
                    conn.execute(
                        "INSERT OR IGNORE INTO confirmations "
                        "(confirm_id, trade_id, symbol, side, quantity, price, venue, status, fees, timestamp_ns) "
                        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
                        (
                            row.get("confirm_id", f"CFM_{row['trade_id']}"),
                            row["trade_id"],
                            row["symbol"],
                            1 if row.get("side", "Buy") == "Buy" else 0,
                            int(row.get("quantity", 0)),
                            float(row.get("price", 0)),
                            row.get("venue", "NYSE"),
                            row.get("status", "confirmed"),
                            float(row.get("fees", 0)),
                            int(row.get("timestamp_ns", 0)),
                        ),
                    )
                count += 1
            except (KeyError, ValueError) as e:
                logger.warning("Skipping row: %s", e)
                continue

    conn.commit()
    conn.close()
    return count


def import_fix(filepath: str, rec_type: str, cfg: dict) -> int:
    """Import records from a FIX message file (one message per line)."""
    conn = _get_conn(cfg)
    count = 0
    soh = "\x01"

    with open(filepath) as f:
        for line in f:
            line = line.strip()
            if not line:
                continue

            tags = {}
            for part in line.split(soh):
                if "=" in part:
                    tag_str, val = part.split("=", 1)
                    try:
                        tags[int(tag_str)] = val
                    except ValueError:
                        continue

            msg_type = tags.get(35, "")
            if msg_type == "D":
                try:
                    side_val = 1 if tags.get(54, "1") == "1" else 0
                    conn.execute(
                        "INSERT OR IGNORE INTO confirmations "
                        "(confirm_id, trade_id, symbol, side, quantity, price, venue, status, fees, timestamp_ns) "
                        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
                        (
                            tags.get(11, f"FIX_{count}"),
                            tags.get(11, f"FIX_{count}"),
                            tags.get(55, ""),
                            side_val,
                            int(tags.get(38, 0)),
                            float(tags.get(44, 0)),
                            tags.get(49, "UNKNOWN"),
                            "confirmed",
                            0.0,
                            0,
                        ),
                    )
                    count += 1
                except (ValueError, KeyError) as e:
                    logger.warning("Skipping FIX message: %s", e)

    conn.commit()
    conn.close()
    return count


def import_json(filepath: str, rec_type: str, cfg: dict) -> int:
    """Import records from a JSON file."""
    conn = _get_conn(cfg)
    count = 0

    with open(filepath) as f:
        data = json.load(f)

    records = data if isinstance(data, list) else data.get("records", [data])

    for row in records:
        try:
            if rec_type == "internal":
                conn.execute(
                    "INSERT OR IGNORE INTO internal_trades "
                    "(trade_id, timestamp_ns, symbol, side, quantity, price, venue, strategy, account, status) "
                    "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
                    (
                        row["trade_id"],
                        int(row.get("timestamp_ns", 0)),
                        row["symbol"],
                        1 if row.get("side", "Buy") == "Buy" else 0,
                        int(row.get("quantity", 0)),
                        float(row.get("price", 0)),
                        row.get("venue", "NYSE"),
                        row.get("strategy", ""),
                        row.get("account", ""),
                        row.get("status", "active"),
                    ),
                )
            elif rec_type == "confirmation":
                conn.execute(
                    "INSERT OR IGNORE INTO confirmations "
                    "(confirm_id, trade_id, symbol, side, quantity, price, venue, status, fees, timestamp_ns) "
                    "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
                    (
                        row.get("confirm_id", f"CFM_{row['trade_id']}"),
                        row["trade_id"],
                        row["symbol"],
                        1 if row.get("side", "Buy") == "Buy" else 0,
                        int(row.get("quantity", 0)),
                        float(row.get("price", 0)),
                        row.get("venue", "NYSE"),
                        row.get("status", "confirmed"),
                        float(row.get("fees", 0)),
                        int(row.get("timestamp_ns", 0)),
                    ),
                )
            count += 1
        except (KeyError, ValueError) as e:
            logger.warning("Skipping JSON record: %s", e)

    conn.commit()
    conn.close()
    return count
