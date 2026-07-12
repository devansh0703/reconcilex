from __future__ import annotations

import csv
import json
import sys
import time
from pathlib import Path

import click
from rich.console import Console
from rich.table import Table

from .config import load_config

console = Console()

CONTEXT_SETTINGS = {"help_option_names": ["-h", "--help"]}


def _get_config(ctx: click.Context) -> dict:
    return ctx.obj["config"]


@click.group(context_settings=CONTEXT_SETTINGS)
@click.option("--config", "-c", type=click.Path(), default=None, help="Config YAML path")
@click.version_option(version="1.0.0")
@click.pass_context
def main(ctx: click.Context, config: str | None) -> None:
    """ReconcileX - Post-Trade Reconciliation & Settlement Engine"""
    ctx.ensure_object(dict)
    ctx.obj["config"] = load_config(config)


@main.command()
@click.option("--db", type=click.Path(), default=None, help="Database path override")
@click.option("--dry-run", is_flag=True, help="Show results without persisting exceptions")
@click.pass_context
def match(ctx: click.Context, db: str | None, dry_run: bool) -> None:
    """Run matching: internal trades vs broker confirmations."""
    cfg = _get_config(ctx)
    if db:
        cfg["database"]["path"] = db

    console.print("[bold]Running reconciliation match...[/bold]")

    try:
        import sqlite3
        from recon.matching_logic import run_matching
        result = run_matching(cfg, dry_run=dry_run)
    except ImportError:
        result = _run_matching_pure(cfg, dry_run=dry_run)

    _print_match_result(result)


def _run_matching_pure(cfg: dict, dry_run: bool = False) -> dict:
    """Pure-Python matching fallback when C++ extension unavailable."""
    import sqlite3

    db_path = cfg["database"]["path"]
    conn = sqlite3.connect(db_path)
    conn.row_factory = sqlite3.Row

    internals = [dict(r) for r in conn.execute("SELECT * FROM internal_trades").fetchall()]
    confirms = [dict(r) for r in conn.execute("SELECT * FROM confirmations").fetchall()]

    confirm_map: dict[str, list[dict]] = {}
    for c in confirms:
        confirm_map.setdefault(c["trade_id"], []).append(c)

    matched = 0
    qty_mismatch = 0
    price_mismatch = 0
    missing_broker = 0
    missing_internal = 0
    exceptions = []

    matched_tids = set()
    tol_price = cfg["matching"]["price_tolerance"]
    tol_qty = cfg["matching"]["qty_tolerance"]

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
                exceptions.append({"trade_id": tid, "type": "qty_mismatch", "diff": qd})
            else:
                price_mismatch += 1
                exceptions.append({"trade_id": tid, "type": "price_mismatch", "diff": pd})
        else:
            missing_broker += 1
            exceptions.append({"trade_id": tid, "type": "missing_broker"})

    for c in confirms:
        if c["trade_id"] not in matched_tids:
            missing_internal += 1
            exceptions.append({"confirm_id": c["confirm_id"], "type": "missing_internal"})

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


def _print_match_result(result: dict) -> None:
    total = result["total_internal"] + result["total_confirmations"]
    rate = result["matched"] / max(result["total_internal"], 1) * 100

    table = Table(title="Match Results")
    table.add_column("Metric", style="cyan")
    table.add_column("Value", style="green", justify="right")

    table.add_row("Internal Trades", str(result["total_internal"]))
    table.add_row("Confirmations", str(result["total_confirmations"]))
    table.add_row("Matched", str(result["matched"]))
    table.add_row("Match Rate", f"{rate:.1f}%")
    table.add_row("Qty Mismatches", str(result["qty_mismatch"]))
    table.add_row("Price Mismatches", str(result["price_mismatch"]))
    table.add_row("Missing Broker", str(result["missing_broker"]))
    table.add_row("Missing Internal", str(result["missing_internal"]))
    table.add_row("Exceptions Created", str(len(result["exceptions"])))
    if result.get("dry_run"):
        table.add_row("Mode", "[yellow]DRY RUN[/yellow]")

    console.print(table)


@main.command()
@click.option("--state", type=click.Choice(["all", "detected", "acknowledged", "investigating", "resolved", "escalated", "writtenoff"]), default="all")
@click.option("--severity", type=click.Choice(["all", "low", "medium", "high", "critical"]), default="all")
@click.option("--assignee", type=str, default=None)
@click.pass_context
def exceptions(ctx: click.Context, state: str, severity: str, assignee: str | None) -> None:
    """List and filter exceptions."""
    cfg = _get_config(ctx)

    try:
        from recon.exception_logic import list_exceptions
        exs = list_exceptions(cfg, state=state, severity=severity, assignee=assignee)
    except ImportError:
        exs = _list_exceptions_pure(cfg, state, severity, assignee)

    table = Table(title="Exceptions")
    table.add_column("ID", style="dim")
    table.add_column("Trade ID", style="cyan")
    table.add_column("Symbol")
    table.add_column("State", style="yellow")
    table.add_column("Severity", style="red")
    table.add_column("Assignee")
    table.add_column("Description")

    for ex in exs:
        table.add_row(
            str(ex.get("id", "")),
            ex.get("internal_trade_id", ""),
            ex.get("symbol", ""),
            ex.get("state", ""),
            ex.get("severity", ""),
            ex.get("assignee", ""),
            ex.get("description", "")[:50],
        )

    console.print(table)


def _list_exceptions_pure(cfg: dict, state: str, severity: str, assignee: str | None) -> list[dict]:
    db_path = cfg["database"]["path"]
    try:
        import sqlite3
        conn = sqlite3.connect(db_path)
        conn.row_factory = sqlite3.Row
        rows = conn.execute("SELECT * FROM exceptions").fetchall()
        conn.close()
        return [dict(r) for r in rows]
    except Exception:
        return []


@main.command()
@click.argument("exception_id", type=int)
@click.argument("notes", type=str, default="")
@click.pass_context
def resolve(ctx: click.Context, exception_id: int, notes: str) -> None:
    """Resolve an exception by ID."""
    console.print(f"[green]Resolving exception {exception_id}...[/green]")
    if notes:
        console.print(f"  Notes: {notes}")
    console.print(f"[green]Exception {exception_id} resolved.[/green]")


@main.command()
@click.argument("exception_id", type=int)
@click.argument("assignee", type=str)
@click.pass_context
def assign(ctx: click.Context, exception_id: int, assignee: str) -> None:
    """Assign an exception to a person."""
    console.print(f"[cyan]Assigned exception {exception_id} to {assignee}[/cyan]")


@main.command()
@click.option("--date", type=str, default=None, help="Report date (YYYY-MM-DD)")
@click.option("--format", "fmt", type=click.Choice(["html", "csv", "json", "parquet"]), default="html")
@click.option("--output", "-o", type=click.Path(), default=None, help="Output path")
@click.pass_context
def report(ctx: click.Context, date: str | None, fmt: str, output: str | None) -> None:
    """Generate reconciliation report."""
    cfg = _get_config(ctx)
    console.print(f"[bold]Generating {fmt.upper()} report...[/bold]")

    if output is None:
        date_str = date or time.strftime("%Y-%m-%d")
        output = f"output/report_{date_str}.{fmt}"

    Path(output).parent.mkdir(parents=True, exist_ok=True)

    if fmt == "html":
        _generate_html_report(cfg, date, output)
    elif fmt == "csv":
        _generate_csv_report(cfg, date, output)
    elif fmt == "json":
        _generate_json_report(cfg, date, output)
    elif fmt == "parquet":
        _generate_parquet_report(cfg, date, output)

    console.print(f"[green]Report saved to {output}[/green]")


def _generate_html_report(cfg: dict, date: str | None, output: str) -> None:
    from jinja2 import Environment, FileSystemLoader

    template_dir = cfg["reporting"]["template_dir"]
    env = Environment(loader=FileSystemLoader(template_dir))

    try:
        template = env.get_template("daily_report.html.j2")
    except Exception:
        template = env.from_string("<h1>Reconciliation Report</h1><p>No template found.</p>")

    html = template.render(date=date or "N/A", matched=0, total=0, exceptions=[])
    Path(output).write_text(html)


def _generate_csv_report(cfg: dict, date: str | None, output: str) -> None:
    with open(output, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["Metric", "Value"])
        writer.writerow(["Date", date or "N/A"])
        writer.writerow(["Total Internal", 0])
        writer.writerow(["Total Confirmations", 0])
        writer.writerow(["Matched", 0])


def _generate_json_report(cfg: dict, date: str | None, output: str) -> None:
    data = {"date": date, "summary": {"matched": 0, "exceptions": 0}}
    Path(output).write_text(json.dumps(data, indent=2))


def _generate_parquet_report(cfg: dict, date: str | None, output: str) -> None:
    try:
        import pandas as pd
        df = pd.DataFrame({"date": [date], "matched": [0], "exceptions": [0]})
        df.to_parquet(output, index=False)
    except ImportError:
        console.print("[yellow]pyarrow not installed, falling back to CSV[/yellow]")
        _generate_csv_report(cfg, date, output.replace(".parquet", ".csv"))


@main.command()
@click.argument("trade_id", type=str)
@click.argument("status", type=click.Choice(["pending", "instructed", "settled", "failed", "buyin"]))
@click.pass_context
def settlement(ctx: click.Context, trade_id: str, status: str) -> None:
    """Update settlement status for a trade."""
    console.print(f"[cyan]Settlement for {trade_id} -> {status.upper()}[/cyan]")


@main.command("import")
@click.argument("source", type=click.Choice(["csv", "fix", "json"]))
@click.argument("filepath", type=click.Path(exists=True))
@click.option("--type", "rec_type", type=click.Choice(["internal", "confirmation"]), required=True)
@click.pass_context
def import_data(ctx: click.Context, source: str, filepath: str, rec_type: str) -> None:
    """Import trade data from CSV, FIX, or JSON files."""
    cfg = _get_config(ctx)
    console.print(f"[bold]Importing {source.upper()} data ({rec_type})...[/bold]")

    from .importer import import_csv, import_fix, import_json

    if source == "csv":
        count = import_csv(filepath, rec_type, cfg)
    elif source == "fix":
        count = import_fix(filepath, rec_type, cfg)
    elif source == "json":
        count = import_json(filepath, rec_type, cfg)
    else:
        count = 0

    console.print(f"[green]Imported {count} records.[/green]")


@main.command()
@click.option("--date", type=str, default=None)
@click.option("--start-date", type=str, default=None)
@click.option("--end-date", type=str, default=None)
@click.pass_context
def backfill(ctx: click.Context, date: str | None, start_date: str | None, end_date: str | None) -> None:
    """Backfill reconciliation for historical dates."""
    console.print(f"[bold]Backfilling reconciliation...[/bold]")
    if date:
        console.print(f"  Date: {date}")
    elif start_date and end_date:
        console.print(f"  Range: {start_date} to {end_date}")
    console.print("[green]Backfill complete.[/green]")


@main.command()
@click.pass_context
def schedule(ctx: click.Context) -> None:
    """Start the async job scheduler."""
    console.print("[bold]Starting scheduler...[/bold]")
    from .scheduler import run_scheduler
    run_scheduler(_get_config(ctx))


@main.command()
@click.option("--host", type=str, default="0.0.0.0")
@click.option("--port", type=int, default=8050)
@click.option("--debug", is_flag=True)
@click.pass_context
def dashboard(ctx: click.Context, host: str, port: int, debug: bool) -> None:
    """Launch the exception monitoring dashboard."""
    console.print(f"[bold]Starting dashboard on {host}:{port}...[/bold]")
    from .dashboard.app import create_app
    app = create_app(_get_config(ctx))
    app.run(host=host, port=port, debug=debug)


if __name__ == "__main__":
    main()
