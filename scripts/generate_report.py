#!/usr/bin/env python3
"""CLI shortcut for generating reconciliation reports."""
import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent))

from recon.config import load_config
from recon.report_renderer import ReportRenderer
from recon.importer import import_csv
from recon.cli import _run_matching_pure


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate ReconcileX reports")
    parser.add_argument("--date", default=None, help="Report date (YYYY-MM-DD)")
    parser.add_argument("--format", choices=["html", "csv", "json"], default="html")
    parser.add_argument("--config", default=None, help="Config path")
    parser.add_argument("--output", default=None, help="Output path")
    args = parser.parse_args()

    from datetime import date as dt
    report_date = args.date or dt.today().isoformat()

    cfg = load_config(args.config)

    # Import data if database is empty
    db_path = cfg["database"]["path"]
    if not Path(db_path).exists():
        print("Importing sample data...")
        import csv
        data_dir = Path(__file__).parent.parent / "data"
        if (data_dir / "sample_internal.csv").exists():
            import_csv(str(data_dir / "sample_internal.csv"), "internal", cfg)
        if (data_dir / "sample_confirmations.csv").exists():
            import_csv(str(data_dir / "sample_confirmations.csv"), "confirmation", cfg)

    # Run matching
    result = _run_matching_pure(cfg)

    # Render report
    renderer = ReportRenderer(
        template_dir=cfg["reporting"]["template_dir"],
        output_dir=cfg["reporting"]["output_dir"],
    )

    result["match_rate"] = result["matched"] / max(result["total_internal"], 1) * 100

    html = renderer.render_daily_report(report_date, result)

    if args.output:
        output_path = Path(args.output)
    else:
        output_path = Path(cfg["reporting"]["output_dir"]) / f"report_{report_date}.{args.format}"

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(html)
    print(f"Report saved to {output_path}")


if __name__ == "__main__":
    main()
