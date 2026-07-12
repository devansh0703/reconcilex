from __future__ import annotations

import json
import tempfile
from pathlib import Path

from click.testing import CliRunner

from recon.cli import main


def test_cli_help():
    runner = CliRunner()
    result = runner.invoke(main, ["--help"])
    assert result.exit_code == 0
    assert "ReconcileX" in result.output


def test_cli_version():
    runner = CliRunner()
    result = runner.invoke(main, ["--version"])
    assert result.exit_code == 0
    assert "1.0.0" in result.output


def test_cli_match_empty_db():
    runner = CliRunner()
    with runner.isolated_filesystem():
        Path("config").mkdir(exist_ok=True)
        Path("config/default.yaml").write_text("database:\n  path: /tmp/test_cli.db\n")
        result = runner.invoke(main, ["-c", "config/default.yaml", "match"])
        assert result.exit_code == 0
        assert "Match Results" in result.output or "Running" in result.output


def test_cli_exceptions():
    runner = CliRunner()
    with runner.isolated_filesystem():
        Path("config").mkdir(exist_ok=True)
        Path("config/default.yaml").write_text("database:\n  path: /tmp/test_exc.db\n")
        result = runner.invoke(main, ["-c", "config/default.yaml", "exceptions"])
        assert result.exit_code == 0


def test_cli_resolve():
    runner = CliRunner()
    result = runner.invoke(main, ["resolve", "1", "Fixed the issue"])
    assert result.exit_code == 0
    assert "Resolving" in result.output


def test_cli_assign():
    runner = CliRunner()
    result = runner.invoke(main, ["assign", "1", "john.doe"])
    assert result.exit_code == 0
    assert "Assigned" in result.output


def test_cli_settlement():
    runner = CliRunner()
    result = runner.invoke(main, ["settlement", "T001", "settled"])
    assert result.exit_code == 0
    assert "Settlement" in result.output


def test_cli_report_html():
    runner = CliRunner()
    with runner.isolated_filesystem():
        Path("templates").mkdir(exist_ok=True)
        Path("config").mkdir(exist_ok=True)
        Path("config/default.yaml").write_text(
            "database:\n  path: /tmp/test_report.db\n"
            "reporting:\n  template_dir: templates\n  output_dir: output\n"
        )
        result = runner.invoke(main, ["-c", "config/default.yaml", "report", "--format", "html"])
        assert result.exit_code == 0
        assert "saved" in result.output.lower() or "Generating" in result.output


def test_cli_report_csv():
    runner = CliRunner()
    with runner.isolated_filesystem():
        Path("config").mkdir(exist_ok=True)
        Path("config/default.yaml").write_text("database:\n  path: /tmp/test_csv.db\n")
        result = runner.invoke(main, ["-c", "config/default.yaml", "report", "--format", "csv"])
        assert result.exit_code == 0
