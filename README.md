# ReconcileX

Post-trade reconciliation and settlement engine. Matches internal trade records against broker/exchange confirmations, manages exception workflows with state machine tracking, and generates reconciliation reports. Implemented in C++20 (matching core) with Python (workflow orchestration, CLI, dashboard).

## Overview

ReconcileX handles the post-trade lifecycle: importing trade records from internal systems and broker confirmations, matching them to detect discrepancies, managing exceptions through investigation and resolution, and tracking settlement status through T+0, T+1, and fail management.

The matching engine uses O(1) hash-based lookup on trade IDs with fuzzy fallback on (symbol, side, quantity, price, timestamp). Discrepancies are classified by type (quantity mismatch, price mismatch, missing on either side) and severity, then routed through a state machine workflow with SLA tracking and aging reports.

The system includes a FIX 4.4 protocol handler for importing broker confirmations, a Jinja2-based report generator for daily reconciliation reports, and a Plotly Dash dashboard for real-time exception monitoring.

## Architecture

```
Trade Sources                    Broker Sources
    |                                |
    v                                v
TradeStore (SQLite WAL)    TradeStore (SQLite WAL)
    |                                |
    +--------+  +--------------------+
             |  |
             v  v
        MatchingEngine
        (hash-based O(1) + fuzzy fallback)
             |
     +-------+-------+
     v               v
MatchResult      Exception
     |           Workflow
     v               |
Settlement      State Machine
Tracker         (Detected -> Acknowledged ->
     |          Investigating -> Resolved /
     v          Escalated / WrittenOff)
Reporting           |
Engine         SLA Tracking
     |          Aging Report
     v               |
HTML/PDF/CSV    Dashboard
Reports         (Plotly Dash)
```

## Features

### Trade Record Store

- Internal trade records: trade_id, timestamp, symbol, side, quantity, price, venue, strategy, account, status
- Broker confirmations: confirm_id, trade_id, symbol, side, quantity, price, venue, status, fees
- SQLite with WAL mode for concurrent reads during writes
- Indexed by (trade_id, venue, date) for fast queries
- Deduplication: composite key on trade_id + venue, rejects duplicates with alert
- Import from CSV, FIX messages, or JSON

### Matching Engine

Two-sided matching of internal records against broker confirmations:

| Match Criteria | Method |
|----------------|--------|
| Primary | trade_id (O(1) hash lookup) |
| Fuzzy fallback | symbol + side + quantity + price + timestamp (within tolerance) |

Match states:

| State | Description |
|-------|-------------|
| Matched | Both sides agree on all fields, auto-settled |
| Qty mismatch | Same trade, different quantities |
| Price mismatch | Same trade, different prices |
| Missing internal | Broker has trade, internal does not (potential unauthorized trade) |
| Missing broker | Internal has trade, broker does not (potential failed settlement) |
| Duplicate | Duplicate confirmation detected |

Auto-resolution: discrepancies below configurable thresholds (e.g., price within 0.01, quantity within 1 share) are automatically resolved with audit log entry.

Batch matching processes all trades for a day in a single pass and produces an exception report.

### Exception Workflow

State machine for each exception:

```
Detected --> Acknowledged --> Investigating --> Resolved
                                         --> Escalated
                                         --> WrittenOff
```

Exception severity classification:

| Type | Severity |
|------|----------|
| Price mismatch > 1bp | High |
| Quantity mismatch > 1 share | High |
| Missing internal | Critical |
| Missing broker | Medium |
| Duplicate | Low |

Features:
- Assignment to operations team members
- Resolution notes (free-text investigation findings)
- SLA tracking: time-to-resolution, escalation deadlines
- Aging report: bucketed by age (0-1 day, 1-3 days, 3-7 days, 7+ days)

### Settlement Tracker

Settlement lifecycle:

```
T+0 (trade date) --> Pending --> Instructed --> Settled
                                          --> Failed --> Buy-in
```

Tracks: cash settlement, security settlement, margin requirements. Flags unsettled trades after T+N deadline. Daily settlement summary: total settled, total pending, total fails, fail rate, breakdown by venue.

### FIX Protocol Handler

Simplified FIX 4.4 message parser and serializer:

| Message Type | Tag | Purpose |
|-------------|-----|---------|
| NewOrderSingle | D | New order submission |
| ExecutionReport | 8 | Trade execution confirmation |
| OrderCancelRequest | F | Order cancellation |
| TradeCaptureReport | AE | Trade capture confirmation |

Features:
- Tag-value parsing with SOH delimiters
- Checksum validation (BodySum % 256)
- Session management: logon/logon-accept, heartbeats, sequence number tracking
- Graceful handling of malformed messages

### Reporting Engine

Daily reconciliation report sections:
- Summary: total trades, matched percentage, exceptions by type
- Matched trades: list with settlement status
- Exceptions: grouped by type, severity, age
- Settlement status: pending/settled/failed counts

Report formats:
- HTML: styled with tables, charts, color-coded severity
- PDF: printable version via WeasyPrint
- CSV: raw data for downstream systems
- JSON: API-consumable format

### Workflow Scheduler

Python asyncio-based scheduler for daily batch jobs:

| Time | Job |
|------|-----|
| 6:00 AM | Import broker confirmations from FTP/API |
| 6:30 AM | Run matching engine on previous day's trades |
| 7:00 AM | Generate reconciliation report, email to ops team |
| 8:00 AM | Check settlement status, flag fails |
| 4:00 PM | End-of-day summary |

Configurable via YAML. Supports manual trigger and backfill.

## CLI

```bash
recon match       --date 2025-06-15 --venue BINANCE
recon match       --date 2025-06-15 --all-venues
recon exceptions  --status open --severity high
recon resolve     <exception_id> --note "Broker confirmed qty" --resolution qty_corrected
recon report      --date 2025-06-15 --format html
recon settlement  --date 2025-06-15
recon import      --source broker_fix.csv --venue BINANCE
recon import      --source internal.csv --type internal
recon backfill    --from 2025-01-01 --to 2025-06-01
recon schedule    --config daily.yaml
recon dashboard
```

## Getting Started

### Prerequisites

- C++20 compiler (GCC 12+ or Clang 15+)
- CMake 3.20+
- SQLite3
- Python 3.10+ (for CLI, scheduler, dashboard)

### Build and Run

```bash
# Build C++ core
chmod +x scripts/build.sh && scripts/build.sh

# Run tests
chmod +x scripts/run_tests.sh && scripts/run_tests.sh

# Setup Python environment
chmod +x scripts/setup_python.sh && scripts/setup_python.sh
source .venv/bin/activate

# Generate sample data
python scripts/seed_data.py

# Run reconciliation
python -m recon.cli match

# Generate report
python -m recon.cli report --date 2025-06-15 --format html

# Launch dashboard
python -m recon.cli dashboard
```

### Docker

```bash
docker-compose up --build
# Includes ReconcileX, SQLite, and demo data
```

## Configuration

| File | Purpose |
|------|---------|
| `config/default.yaml` | Matching thresholds, SLA deadlines, auto-resolution rules |
| `config/venues.yaml` | Venue definitions, FIX endpoints |
| `config/schedule.yaml` | Daily job schedule |

## Tech Stack

| Layer | Technology |
|-------|------------|
| Matching Core | C++20, std::unordered_map, std::map |
| Storage | SQLite (WAL mode) |
| Protocol | FIX 4.4 (parser/serializer) |
| Workflow | Python asyncio scheduler |
| CLI | Python Click |
| Reports | Jinja2 (HTML), WeasyPrint (PDF), CSV, JSON |
| Dashboard | Plotly Dash, WebSocket |
| Testing | Google Test (C++), pytest (Python) |
| Build | CMake (C++), pyproject.toml (Python) |

## Testing

C++ test suite (8 suites):

- Trade store: add, query, dedup, concurrent access
- Matching engine: exact match, qty mismatch, price mismatch, missing on each side, fuzzy match
- Exception workflow: all state transitions, SLA calculation, aging
- Settlement tracker: lifecycle transitions, fail detection
- FIX handler: every message type, malformed messages, checksum validation
- Reporting: HTML output contains expected sections
- Integration: 1000 internal trades + 995 confirmations + 5 exceptions, full pipeline verification
- Fuzz: 10k random FIX messages, parser handles gracefully

Python test suite (3 suites): CLI commands, CSV/FIX import, scheduler jobs.

## Project Structure

```
reconcilex/
  include/recon/      Headers: TradeRecord, Confirmation, TradeStore, MatchingEngine,
                       Exception, ExceptionWorkflow, Settlement, FixHandler, Reporting
  include/fix/        FIX 4.4 tag definitions
  src/                C++ implementations (~2500 lines)
  test/               Google Test (8 suites, including integration and fuzz)
  python/recon/       CLI, scheduler, importer, report renderer, dashboard
  templates/          Jinja2 HTML templates (daily report, aging, settlement)
  config/             YAML: thresholds, venues, schedule
  data/               Sample trades (5k), confirmations (5k), FIX messages
  scripts/            Build, test, Python setup, data generation
  .github/workflows/  CI pipeline
```
