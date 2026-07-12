from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from jinja2 import Environment, FileSystemLoader, TemplateNotFound


class ReportRenderer:
    """Renders reconciliation reports using Jinja2 templates."""

    def __init__(self, template_dir: str = "templates", output_dir: str = "output") -> None:
        self.template_dir = Path(template_dir)
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(parents=True, exist_ok=True)

        self.env = Environment(
            loader=FileSystemLoader(str(self.template_dir)),
            autoescape=True,
        )

    def render_daily_report(self, date: str, data: dict[str, Any]) -> str:
        try:
            template = self.env.get_template("daily_report.html.j2")
        except TemplateNotFound:
            template = self.env.from_string(self._fallback_daily(date, data))

        html = template.render(
            date=date,
            total_internal=data.get("total_internal", 0),
            total_confirmations=data.get("total_confirmations", 0),
            matched=data.get("matched", 0),
            match_rate=data.get("match_rate", 0),
            qty_mismatch=data.get("qty_mismatch", 0),
            price_mismatch=data.get("price_mismatch", 0),
            missing_broker=data.get("missing_broker", 0),
            missing_internal=data.get("missing_internal", 0),
            exceptions=data.get("exceptions", []),
        )
        return html

    def render_exception_aging(self, aging: dict[str, int], exceptions: list[dict]) -> str:
        try:
            template = self.env.get_template("exception_aging.html.j2")
        except TemplateNotFound:
            template = self.env.from_string(self._fallback_aging(aging, exceptions))

        return template.render(aging=aging, exceptions=exceptions)

    def render_settlement_summary(self, date: str, summary: dict[str, Any],
                                   records: list[dict]) -> str:
        try:
            template = self.env.get_template("settlement_summary.html.j2")
        except TemplateNotFound:
            template = self.env.from_string(self._fallback_settlement(date, summary, records))

        return template.render(date=date, summary=summary, records=records)

    def save_html(self, html: str, filename: str) -> Path:
        path = self.output_dir / filename
        path.write_text(html)
        return path

    def _fallback_daily(self, date: str, data: dict) -> str:
        return f"""<!DOCTYPE html>
<html><head><title>Reconciliation Report - {date}</title></head>
<body><h1>Reconciliation Report - {date}</h1>
<pre>{json.dumps(data, indent=2)}</pre></body></html>"""

    def _fallback_aging(self, aging: dict, exceptions: list) -> str:
        return f"""<!DOCTYPE html>
<html><head><title>Exception Aging</title></head>
<body><h1>Exception Aging</h1>
<pre>{json.dumps(aging, indent=2)}</pre>
<h2>Exceptions</h2><pre>{json.dumps(exceptions, indent=2)}</pre></body></html>"""

    def _fallback_settlement(self, date: str, summary: dict, records: list) -> str:
        return f"""<!DOCTYPE html>
<html><head><title>Settlement - {date}</title></head>
<body><h1>Settlement Summary - {date}</h1>
<pre>{json.dumps(summary, indent=2)}</pre>
<h2>Records</h2><pre>{json.dumps(records, indent=2)}</pre></body></html>"""
