from __future__ import annotations

from typing import Any

try:
    import dash
    from dash import dcc, html
    from dash.dependencies import Input, Output
except ImportError:
    dash = None  # type: ignore

from .panels import exception_list_panel, aging_chart_panel, settlement_panel


def create_app(config: dict[str, Any]) -> Any:
    """Create the Plotly Dash application."""
    if dash is None:
        raise ImportError("dash is required for the dashboard")

    app = dash.Dash(__name__, title="ReconcileX Dashboard")

    app.layout = html.Div([
        html.H1("ReconcileX - Exception Dashboard", style={"textAlign": "center", "color": "#1a237e"}),

        dcc.Tabs(id="tabs", value="exceptions", children=[
            dcc.Tab(label="Exceptions", value="exceptions"),
            dcc.Tab(label="Aging", value="aging"),
            dcc.Tab(label="Settlement", value="settlement"),
        ]),

        html.Div(id="tab-content"),
    ])

    @app.callback(Output("tab-content", "children"), Input("tabs", "value"))
    def render_tab(tab: str) -> html.Div:
        if tab == "exceptions":
            return exception_list_panel(config)
        elif tab == "aging":
            return aging_chart_panel(config)
        elif tab == "settlement":
            return settlement_panel(config)
        return html.Div("Unknown tab")

    return app
