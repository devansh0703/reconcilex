#include <gtest/gtest.h>
#include "recon/trade_store.h"
#include "recon/matching_engine.h"
#include "recon/exception_workflow.h"
#include "recon/settlement.h"
#include "recon/fix_handler.h"
#include "recon/reporting.h"
#include <fix/fix44.h>

using namespace recon;

class IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        store = std::make_unique<TradeStore>(":memory:");

        // Insert 1000 internal trades
        store->begin_transaction();
        for (int i = 0; i < 1000; i++) {
            TradeRecord r;
            r.trade_id = "INT_" + std::to_string(i);
            r.timestamp_ns = 1700000000000000000ULL + i * 1000000;
            r.symbol = (i % 3 == 0) ? "AAPL" : (i % 3 == 1) ? "GOOG" : "MSFT";
            r.side = (i % 2 == 0) ? Side::Buy : Side::Sell;
            r.quantity = 100 + (i % 10) * 10;
            r.price = 150.0 + (i % 50);
            r.venue = (i % 2 == 0) ? "NYSE" : "NASDAQ";
            r.strategy = "stat_arb";
            r.account = "ACC_" + std::to_string(i % 10);
            r.status = "active";
            store->add_internal(r);
        }
        store->commit();

        // Insert 995 confirmations (5 missing, plus 5 with qty/price mismatches)
        store->begin_transaction();
        for (int i = 0; i < 1000; i++) {
            if (i >= 995 && i < 1000) continue; // 5 missing confirmations

            BrokerConfirmation c;
            c.confirm_id = "CFM_" + std::to_string(i);
            c.trade_id = "INT_" + std::to_string(i);
            c.symbol = (i % 3 == 0) ? "AAPL" : (i % 3 == 1) ? "GOOG" : "MSFT";
            c.side = (i % 2 == 0) ? Side::Buy : Side::Sell;
            c.quantity = 100 + (i % 10) * 10;
            c.price = 150.0 + (i % 50);

            // Introduce mismatches for trades 100-104
            if (i >= 100 && i <= 102) c.quantity += 10; // qty mismatch
            if (i >= 103 && i <= 104) c.price += 5.0;  // price mismatch

            c.venue = (i % 2 == 0) ? "NYSE" : "NASDAQ";
            c.status = "confirmed";
            c.fees = 1.50;
            c.timestamp_ns = 1700000000000000000ULL + i * 1000000;
            store->add_confirmation(c);
        }
        store->commit();
    }

    std::unique_ptr<TradeStore> store;
};

TEST_F(IntegrationTest, DataIntegrity) {
    EXPECT_EQ(store->get_internal_count(), 1000);
    EXPECT_EQ(store->get_confirmation_count(), 995);
}

TEST_F(IntegrationTest, MatchingPipeline) {
    auto internals = store->query_internal();
    auto confirms = store->query_confirmations();

    MatchingEngine engine;
    auto results = engine.batch_match(internals, confirms);

    size_t matched = 0, mismatches = 0, missing_broker = 0, missing_internal = 0;
    for (const auto& r : results) {
        switch (r.state) {
            case MatchState::Matched:         matched++; break;
            case MatchState::QtyMismatch:     mismatches++; break;
            case MatchState::PriceMismatch:   mismatches++; break;
            case MatchState::MissingBroker:   missing_broker++; break;
            case MatchState::MissingInternal: missing_internal++; break;
            default: break;
        }
    }

    // 5 missing broker confirmations, 5 mismatches (3 qty + 2 price)
    EXPECT_EQ(missing_broker, 5);
    EXPECT_GE(mismatches, 5);
    EXPECT_GE(matched, 990);
}

TEST_F(IntegrationTest, ExceptionCreation) {
    auto internals = store->query_internal();
    auto confirms = store->query_confirmations();

    MatchingEngine engine;
    auto results = engine.batch_match(internals, confirms);

    ExceptionWorkflow workflow;
    for (const auto& r : results) {
        if (r.state != MatchState::Matched) {
            workflow.create_exception(r);
        }
    }

    EXPECT_GE(workflow.total_exceptions(), 10);

    auto aging = workflow.get_aging_report();
    EXPECT_GE(aging["0-1d"], static_cast<int>(workflow.total_exceptions()));
}

TEST_F(IntegrationTest, ExceptionLifecycle) {
    auto internals = store->query_internal();
    auto confirms = store->query_confirmations();

    MatchingEngine engine;
    auto results = engine.batch_match(internals, confirms);

    ExceptionWorkflow workflow;
    for (const auto& r : results) {
        if (r.state != MatchState::Matched) {
            auto ex = workflow.create_exception(r);
            workflow.acknowledge(ex.id);
            workflow.investigate(ex.id);
            workflow.resolve(ex.id, "Root cause identified", "Corrected in OMS");
        }
    }

    int resolved_count = 0;
    for (const auto& ex : workflow.get_exceptions()) {
        if (ex.state == ExceptionState::Resolved) resolved_count++;
    }
    EXPECT_EQ(resolved_count, static_cast<int>(workflow.total_exceptions()));
}

TEST_F(IntegrationTest, SettlementTracking) {
    SettlementTracker settlement;

    auto internals = store->query_internal();
    for (int i = 0; i < 50; i++) {
        settlement.track_settlement(internals[i].trade_id, SettlementStatus::Pending,
                                     internals[i].venue, internals[i].price * internals[i].quantity);
    }

    // Settle first 40
    for (int i = 0; i < 40; i++) {
        settlement.update_status(internals[i].trade_id, SettlementStatus::Instructed);
        settlement.update_status(internals[i].trade_id, SettlementStatus::Settled);
    }

    // Fail 5
    for (int i = 40; i < 45; i++) {
        settlement.update_status(internals[i].trade_id, SettlementStatus::Instructed);
        settlement.update_status(internals[i].trade_id, SettlementStatus::Failed);
    }

    auto summary = settlement.get_summary();
    EXPECT_EQ(summary.total_settled, 40);
    EXPECT_EQ(summary.total_failed, 5);
    EXPECT_EQ(summary.total_pending, 5);
    EXPECT_NEAR(summary.fail_rate, 0.1, 0.01);
}

TEST_F(IntegrationTest, FIXMessagePipeline) {
    FixHandler handler;
    ExceptionWorkflow workflow;

    // Simulate FIX messages for order flow
    std::string order = handler.make_new_order_single("ORD001", "AAPL", 1, 100, 150.50, "TRC", "NYSE", 1);
    auto parsed = handler.parse(order);
    EXPECT_EQ(parsed[fix44::MsgType], fix44::MsgType_NewOrderSingle);

    std::string exec = handler.make_execution_report("EX001", "ORD001", "AAPL", 1, 100, 150.50, "2", "NYSE", "TRC", 2);
    auto exec_parsed = handler.parse(exec);
    EXPECT_EQ(exec_parsed[fix44::ExecType], "2");

    EXPECT_TRUE(handler.validate_checksum(order));
    EXPECT_TRUE(handler.validate_checksum(exec));
}

TEST_F(IntegrationTest, ReportGeneration) {
    auto internals = store->query_internal();
    auto confirms = store->query_confirmations();

    MatchingEngine engine;
    auto results = engine.batch_match(internals, confirms);

    Reporting reporter;
    auto html = reporter.daily_reconciliation_report("2026-01-15", results, 1000, 995);
    EXPECT_TRUE(html.find("Reconciliation Report") != std::string::npos);
    EXPECT_TRUE(html.find("1000") != std::string::npos);
    EXPECT_TRUE(html.find("995") != std::string::npos);
}
