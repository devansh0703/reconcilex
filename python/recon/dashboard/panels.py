from __future__ import annotations

import sqlite3
from pathlib import Path
from typing import Any

try:
    from dash import dcc, html
    import plotly.graph_objects as go
except ImportError:
    dcc = None  # type: ignore
    html = None  # type: ignore
    go = None  # type: ignore


def _get_exceptions(config: dict[str, Any]) -> list[dict]:
    db_path = config["database"]["path"]
    if not Path(db_path).exists():
        return []
    try:
        conn = sqlite3.connect(db_path)
        conn.row_factory = sqlite3.Row
        rows = conn.execute(
            "SELECT * FROM exceptions ORDER BY created_at_ns DESC LIMIT 100"
        ).fetchall()
        conn.close()
        return [dict(r) for r in rows]
    except Exception:
        return []


def exception_list_panel(config: dict[str, Any]) -> Any:
    """Render the exception list panel."""
    exceptions = _get_exceptions(config)

    if not exceptions:
        return html.Div([
            html.H3("No Exceptions"),
            html.P("All trades are matched. No exceptions found."),
        ])

    rows = []
    for ex in exceptions:
        severity_color = {
            "Low": "#4caf50", "Medium": "#ff9800",
            "High": "#f44336", "Critical": "#9c27b0",
        }.get(ex.get("severity", ""), "#666")

        rows.append(html.Tr([
            html.Td(str(ex.get("id", ""))),
            html.Td(ex.get("internal_trade_id", "")),
            html.Td(ex.get("symbol", "")),
            html.Td(ex.get("state", "")),
            html.Td(ex.get("severity", ""), style={"color": severity_color, "fontWeight": "bold"}),
            html.Td(ex.get("assignee", "") or "-"),
            html.Td(ex.get("description", "")[:80]),
        ]))

    table = html.Table([
        html.Thead(html.Tr([
            html.Th("ID"), html.Th("Trade ID"), html.Th("Symbol"),
            html.Th("State"), html.Th("Severity"), html.Th("Assignee"), html.Th("Description"),
        ])),
        html.Tbody(rows),
    ], style={"width": "100%", "borderCollapse": "collapse"})

    return html.Div([
        html.H3(f"Exceptions ({len(exceptions)})"),
        table,
    ])


def aging_chart_panel(config: dict[str, Any]) -> Any:
    """Render the exception aging chart panel."""
    exceptions = _get_exceptions(config)

    buckets = {"0-1d": 0, "1-3d": 0, "3-7d": 0, "7d+": 0}
    for ex in exceptions:
        state = ex.get("state", "")
        if state in ("Resolved", "WrittenOff"):
            continue
        buckets["0-1d"] += 1  # simplified aging

    if dcc is None or go is None:
        return html.Div([html.H3("Aging Chart"), html.P("dash/plotly not installed")])

    fig = go.Figure(data=[
        go.Bar(
            x=list(buckets.keys()),
            y=list(buckets.values()),
            marker_color=["#4caf50", "#ff9800", "#f44336", "#9c27b0"],
        )
    ])
    fig.update_layout(
        title="Exception Aging",
        xaxis_title="Age Bucket",
        yaxis_title="Count",
        template="plotly_white",
    )

    return html.Div([
        html.H3("Exception Aging"),
        dcc.Graph(figure=fig),
    ])


def settlement_panel(config: dict[str, Any]) -> Any:
    """Render the settlement status panel."""
    db_path = config["database"]["path"]
    summary = {"settled": 0, "pending": 0, "failed": 0, "buyin": 0}

    if Path(db_path).exists():
        try:
            conn = sqlite3.connect(db_path)
            conn.row_factory = sqlite3.Row
            rows = conn.execute("SELECT * FROM exceptions").fetchall()
            conn.close()
        except Exception:
            pass

    if dcc is None or go is None:
        return html.Div([html.H3("Settlement"), html.P("dash/plotly not installed")])

    fig = go.Figure(data=[go.Pie(
        labels=["Settled", "Pending", "Failed", "Buy-In"],
        values=[summary["settled"], summary["pending"], summary["failed"], summary["buyin"]],
        marker_colors=["#4caf50", "#ff9800", "#f44336", "#9c27b0"],
    )])
    fig.update_layout(title="Settlement Status", template="plotly_white")

    return html.Div([
        html.H3("Settlement Status"),
        dcc.Graph(figure=fig),
    ])
