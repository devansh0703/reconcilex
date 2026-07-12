#include "recon/reporting.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <algorithm>

namespace recon {

Reporting::Reporting(ReportingConfig config) : config_(std::move(config)) {}

std::string Reporting::daily_reconciliation_report(
    const std::string& date,
    const std::vector<MatchResult>& matches,
    size_t total_internal,
    size_t total_confirmations) const {

    size_t matched = 0, qty_mismatch = 0, price_mismatch = 0;
    size_t missing_internal = 0, missing_broker = 0, duplicate = 0;

    for (const auto& m : matches) {
        switch (m.state) {
            case MatchState::Matched:         matched++; break;
            case MatchState::QtyMismatch:     qty_mismatch++; break;
            case MatchState::PriceMismatch:   price_mismatch++; break;
            case MatchState::MissingInternal: missing_internal++; break;
            case MatchState::MissingBroker:   missing_broker++; break;
            case MatchState::Duplicate:       duplicate++; break;
        }
    }

    double match_rate = matches.empty() ? 0.0 : (static_cast<double>(matched) / matches.size() * 100.0);

    std::ostringstream html;
    html << R"(<!DOCTYPE html><html><head><title>Reconciliation Report - )" << date << R"(</title>
<style>
body{font-family:Arial,sans-serif;margin:20px;background:#f5f5f5}
.container{max-width:1200px;margin:auto;background:white;padding:20px;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1)}
h1{color:#1a237e;border-bottom:2px solid #1a237e;padding-bottom:10px}
.summary{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:15px;margin:20px 0}
.card{background:#f8f9fa;padding:15px;border-radius:8px;border-left:4px solid #1a237e}
.card h3{margin:0;color:#666;font-size:14px}
.card .value{font-size:28px;font-weight:bold;color:#1a237e}
.matched{border-left-color:#4caf50}
.warning{border-left-color:#ff9800}
.error{border-left-color:#f44336}
table{width:100%;border-collapse:collapse;margin:15px 0}
th,td{padding:10px;text-align:left;border-bottom:1px solid #ddd}
th{background:#1a237e;color:white}
tr:hover{background:#f5f5f5}
.status-matched{color:#4caf50;font-weight:bold}
.status-mismatch{color:#ff9800;font-weight:bold}
.status-missing{color:#f44336;font-weight:bold}
</style></head><body>
<div class="container">
<h1>Post-Trade Reconciliation Report</h1>
<p>Date: <strong>)" << date << R"(</strong></p>

<div class="summary">
<div class="card matched"><h3>Internal Trades</h3><div class="value">)" << total_internal << R"(</div></div>
<div class="card matched"><h3>Broker Confirmations</h3><div class="value">)" << total_confirmations << R"(</div></div>
<div class="card matched"><h3>Matched</h3><div class="value">)" << matched << R"(</div></div>
<div class="card warning"><h3>Match Rate</h3><div class="value">)" << std::fixed << std::setprecision(1) << match_rate << R"(%</div></div>
</div>

<div class="summary">
<div class="card warning"><h3>Qty Mismatches</h3><div class="value">)" << qty_mismatch << R"(</div></div>
<div class="card warning"><h3>Price Mismatches</h3><div class="value">)" << price_mismatch << R"(</div></div>
<div class="card error"><h3>Missing Internal</h3><div class="value">)" << missing_internal << R"(</div></div>
<div class="card error"><h3>Missing Broker</h3><div class="value">)" << missing_broker << R"(</div></div>
<div class="card warning"><h3>Duplicates</h3><div class="value">)" << duplicate << R"(</div></div>
</div>

<h2>Match Details</h2>
<table>
<tr><th>Internal ID</th><th>Broker ID</th><th>Symbol</th><th>Status</th><th>Details</th></tr>)";

    for (const auto& m : matches) {
        std::string status_class;
        switch (m.state) {
            case MatchState::Matched:         status_class = "status-matched"; break;
            default:                          status_class = "status-mismatch"; break;
        }
        html << "<tr><td>" << m.internal_trade_id << "</td><td>" << m.broker_trade_id
             << "</td><td>" << m.symbol << "</td><td class=\"" << status_class << "\">"
             << to_string(m.state) << "</td><td>" << m.details << "</td></tr>";
    }

    html << R"(</table></div></body></html>)";
    return html.str();
}

std::string Reporting::exception_aging_report(const std::vector<Exception>& exceptions,
                                               const std::unordered_map<std::string, int>& aging) const {
    std::ostringstream html;
    html << R"(<!DOCTYPE html><html><head><title>Exception Aging Report</title>
<style>
body{font-family:Arial,sans-serif;margin:20px;background:#f5f5f5}
.container{max-width:1000px;margin:auto;background:white;padding:20px;border-radius:8px}
h1{color:#1a237e}
.aging-chart{display:flex;gap:10px;margin:20px 0;flex-wrap:wrap}
.aging-bar{padding:15px;border-radius:8px;text-align:center;min-width:120px;color:white;font-weight:bold}
.bucket-0{background:#4caf50} .bucket-1{background:#ff9800} .bucket-2{background:#f44336} .bucket-3{background:#9c27b0}
table{width:100%;border-collapse:collapse;margin:15px 0}
th,td{padding:8px;border-bottom:1px solid #ddd;text-align:left}
th{background:#1a237e;color:white}
.sev-low{color:#4caf50} .sev-medium{color:#ff9800} .sev-high{color:#f44336} .sev-critical{color:#9c27b0}
</style></head><body>
<div class="container">
<h1>Exception Aging Report</h1>

<div class="aging-chart">)";

    auto add_bucket = [&](const std::string& label, const std::string& css, int count) {
        html << "<div class=\"aging-bar " << css << "\"><div>" << count << "</div><div>" << label << "</div></div>";
    };

    add_bucket("0-1d", "bucket-0", aging.count("0-1d") ? aging.at("0-1d") : 0);
    add_bucket("1-3d", "bucket-1", aging.count("1-3d") ? aging.at("1-3d") : 0);
    add_bucket("3-7d", "bucket-2", aging.count("3-7d") ? aging.at("3-7d") : 0);
    add_bucket("7d+", "bucket-3", aging.count("7d+") ? aging.at("7d+") : 0);

    html << R"(</div>

<h2>Open Exceptions</h2>
<table>
<tr><th>ID</th><th>Trade ID</th><th>Symbol</th><th>State</th><th>Severity</th><th>Assignee</th></tr>)";

    for (const auto& ex : exceptions) {
        std::string sev_class;
        switch (ex.severity) {
            case ExceptionSeverity::Low:      sev_class = "sev-low"; break;
            case ExceptionSeverity::Medium:   sev_class = "sev-medium"; break;
            case ExceptionSeverity::High:     sev_class = "sev-high"; break;
            case ExceptionSeverity::Critical: sev_class = "sev-critical"; break;
        }
        html << "<tr><td>" << ex.id << "</td><td>" << ex.internal_trade_id
             << "</td><td>" << ex.symbol << "</td><td>" << to_string(ex.state)
             << "</td><td class=\"" << sev_class << "\">" << to_string(ex.severity)
             << "</td><td>" << ex.assignee << "</td></tr>";
    }

    html << R"(</table></div></body></html>)";
    return html.str();
}

std::string Reporting::settlement_summary_report(const std::string& date,
                                                  const SettlementSummary& summary,
                                                  const std::vector<SettlementRecord>& records) const {
    std::ostringstream html;
    html << R"(<!DOCTYPE html><html><head><title>Settlement Summary - )" << date << R"(</title>
<style>
body{font-family:Arial,sans-serif;margin:20px;background:#f5f5f5}
.container{max-width:1000px;margin:auto;background:white;padding:20px;border-radius:8px}
h1{color:#1a237e}
.summary{display:grid;grid-template-columns:repeat(4,1fr);gap:15px;margin:20px 0}
.card{padding:15px;border-radius:8px;text-align:center}
.card h3{margin:0;font-size:14px;color:#666}
.card .value{font-size:24px;font-weight:bold}
.settled{background:#e8f5e9;color:#2e7d32} .pending{background:#fff3e0;color:#f57c00}
.failed{background:#ffebee;color:#c62828} .buyin{background:#fce4ec;color:#ad1457}
table{width:100%;border-collapse:collapse;margin:15px 0}
th,td{padding:8px;border-bottom:1px solid #ddd;text-align:left}
th{background:#1a237e;color:white}
</style></head><body>
<div class="container">
<h1>Settlement Summary - )" << date << R"(</h1>

<div class="summary">
<div class="card settled"><h3>Settled</h3><div class="value">)" << summary.total_settled << R"(</div></div>
<div class="card pending"><h3>Pending</h3><div class="value">)" << summary.total_pending << R"(</div></div>
<div class="card failed"><h3>Failed</h3><div class="value">)" << summary.total_failed << R"(</div></div>
<div class="card buyin"><h3>Fail Rate</h3><div class="value">)" << std::fixed << std::setprecision(2)
     << (summary.fail_rate * 100.0) << R"(%</div></div>
</div>

<table>
<tr><th>Trade ID</th><th>Status</th><th>Venue</th><th>Amount</th></tr>)";

    for (const auto& r : records) {
        html << "<tr><td>" << r.trade_id << "</td><td>" << to_string(r.status)
             << "</td><td>" << r.venue << "</td><td>" << std::fixed << std::setprecision(2) << r.amount << "</td></tr>";
    }

    html << R"(</table></div></body></html>)";
    return html.str();
}

void Reporting::export_csv(const std::string& data, const std::string& output_path) {
    std::ofstream out(output_path);
    out << data;
}

void Reporting::export_json(const std::string& data, const std::string& output_path) {
    std::ofstream out(output_path);
    out << data;
}

std::string Reporting::render_template(const std::string& template_name,
                                        const std::string& data_block) const {
    std::ifstream tmpl(config_.template_dir + "/" + template_name);
    if (!tmpl.is_open()) return data_block;
    std::string content((std::istreambuf_iterator<char>(tmpl)), std::istreambuf_iterator<char>());
    // Simple placeholder replacement
    auto pos = content.find("{{ content }}");
    if (pos != std::string::npos) {
        content.replace(pos, 13, data_block);
    }
    return content;
}

} // namespace recon
