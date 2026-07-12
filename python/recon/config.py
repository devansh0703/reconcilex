from __future__ import annotations

import os
from pathlib import Path
from typing import Any

import yaml


_DEFAULT_CONFIG = {
    "database": {"path": "data/reconcilex.db"},
    "matching": {
        "price_tolerance": 0.01,
        "qty_tolerance": 1,
        "auto_resolve": True,
    },
    "sla": {
        "resolution_hours": 24,
        "escalation_hours": 48,
        "aging_buckets": ["0-1d", "1-3d", "3-7d", "7d+"],
    },
    "settlement": {
        "t_plus": 2,
        "fail_threshold_hours": 48,
    },
    "reporting": {
        "template_dir": "templates",
        "output_dir": "output",
    },
    "venues": {
        "NYSE": {"fix_target": "NYSE_FIX", "settlement_t_plus": 2},
        "NASDAQ": {"fix_target": "NASDAQ_FIX", "settlement_t_plus": 2},
    },
    "scheduler": {
        "match_cron": "0 18 * * 1-5",
        "report_cron": "0 20 * * 1-5",
        "settlement_check_cron": "0 9 * * 1-5",
    },
}


def load_config(config_path: str | Path | None = None) -> dict[str, Any]:
    """Load YAML config with defaults. Merges on top of built-in defaults."""
    cfg = dict(_DEFAULT_CONFIG)

    if config_path is None:
        for candidate in ["config/default.yaml", "default.yaml"]:
            p = Path(candidate)
            if p.exists():
                config_path = p
                break

    if config_path and Path(config_path).exists():
        with open(config_path) as f:
            user = yaml.safe_load(f) or {}
        _deep_merge(cfg, user)

    env_db = os.environ.get("RECONX_DB_PATH")
    if env_db:
        cfg["database"]["path"] = env_db

    return cfg


def _deep_merge(base: dict, override: dict) -> dict:
    for k, v in override.items():
        if k in base and isinstance(base[k], dict) and isinstance(v, dict):
            _deep_merge(base[k], v)
        else:
            base[k] = v
    return base
