#include <gtest/gtest.h>
#include "recon/reporting.h"
#include "recon/matching_engine.h"
#include "recon/settlement.h"
#include <fstream>

using namespace recon;

class ReportingTest : public ::testing::Test {
protected:
    Reporting reporter;

    MatchResult make_match(MatchState state, const std::string& tid = "T001") {
        MatchResult r;
        r.state = state;
        r.internal_trade_id = tid;
        r.broker_trade_id = "B_" + tid;
        r.symbol = "AAPL";
        r.details = "Test detail";
        return r;
    }
};

TEST_F(ReportingTest, DailyReportContainsSummary) {
    std::vector<MatchResult> matches = {
        make_match(MatchState::Matched, "T001"),
        make_match(MatchState::QtyMismatch, "T002"),
        make_match(MatchState::PriceMismatch, "T003")
    };

    auto html = reporter.daily_reconciliation_report("2026-01-15", matches, 100, 98);
    EXPECT_TRUE(html.find("Reconciliation Report") != std::string::npos);
    EXPECT_TRUE(html.find("2026-01-15") != std::string::npos);
    EXPECT_TRUE(html.find("100") != std::string::npos);
    EXPECT_TRUE(html.find("Matched") != std::string::npos);
}

TEST_F(ReportingTest, DailyReportEmptyMatches) {
    std::vector<MatchResult> empty;
    auto html = reporter.daily_reconciliation_report("2026-01-15", empty, 0, 0);
    EXPECT_TRUE(html.find("Reconciliation Report") != std::string::npos);
}

TEST_F(ReportingTest, ExceptionAgingReport) {
    Exception ex;
    ex.id = 1;
    ex.match_state = MatchState::QtyMismatch;
    ex.severity = ExceptionSeverity::High;
    ex.state = ExceptionState::Detected;
    ex.internal_trade_id = "T001";
    ex.symbol = "AAPL";
    ex.assignee = "john.doe";

    std::unordered_map<std::string, int> aging = {{"0-1d", 5}, {"1-3d", 3}, {"3-7d", 1}, {"7d+", 0}};

    auto html = reporter.exception_aging_report({ex}, aging);
    EXPECT_TRUE(html.find("Exception Aging Report") != std::string::npos);
    EXPECT_TRUE(html.find("john.doe") != std::string::npos);
    EXPECT_TRUE(html.find("AAPL") != std::string::npos);
}

TEST_F(ReportingTest, SettlementSummaryReport) {
    SettlementSummary summary;
    summary.total_settled = 500;
    summary.total_pending = 50;
    summary.total_failed = 5;
    summary.fail_rate = 0.0099;

    SettlementRecord rec;
    rec.trade_id = "T001";
    rec.status = SettlementStatus::Settled;
    rec.venue = "NYSE";
    rec.amount = 15000.0;

    auto html = reporter.settlement_summary_report("2026-01-15", summary, {rec});
    EXPECT_TRUE(html.find("Settlement Summary") != std::string::npos);
    EXPECT_TRUE(html.find("500") != std::string::npos);
    EXPECT_TRUE(html.find("T001") != std::string::npos);
}

TEST_F(ReportingTest, ExportCsv) {
    reporter.export_csv("col1,col2\nval1,val2\n", "/tmp/test_export.csv");
    std::ifstream f("/tmp/test_export.csv");
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    EXPECT_TRUE(content.find("col1,col2") != std::string::npos);
}

TEST_F(ReportingTest, ExportJson) {
    reporter.export_json("{\"key\": \"value\"}", "/tmp/test_export.json");
    std::ifstream f("/tmp/test_export.json");
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    EXPECT_TRUE(content.find("key") != std::string::npos);
}
