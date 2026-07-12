#include <gtest/gtest.h>
#include "recon/matching_engine.h"

using namespace recon;

class MatchingEngineTest : public ::testing::Test {
protected:
    MatchingEngine engine;

    TradeRecord make_trade(const std::string& id, const std::string& symbol = "AAPL",
                            Side side = Side::Buy, int64_t qty = 100, double price = 150.0) {
        TradeRecord r;
        r.trade_id = id;
        r.timestamp_ns = 1700000000000000000ULL;
        r.symbol = symbol;
        r.side = side;
        r.quantity = qty;
        r.price = price;
        r.venue = "NYSE";
        return r;
    }

    BrokerConfirmation make_confirm(const std::string& tid, const std::string& symbol = "AAPL",
                                     Side side = Side::Buy, int64_t qty = 100, double price = 150.0) {
        BrokerConfirmation c;
        c.confirm_id = "C_" + tid;
        c.trade_id = tid;
        c.symbol = symbol;
        c.side = side;
        c.quantity = qty;
        c.price = price;
        c.venue = "NYSE";
        c.timestamp_ns = 1700000000000000000ULL;
        return c;
    }
};

TEST_F(MatchingEngineTest, ExactMatch) {
    auto trade = make_trade("T001");
    auto confirm = make_confirm("T001");
    auto result = engine.match_single(trade, confirm);
    EXPECT_EQ(result.state, MatchState::Matched);
}

TEST_F(MatchingEngineTest, QtyMismatch) {
    auto trade = make_trade("T001", "AAPL", Side::Buy, 100, 150.0);
    auto confirm = make_confirm("T001", "AAPL", Side::Buy, 200, 150.0);
    auto result = engine.match_single(trade, confirm);
    EXPECT_EQ(result.state, MatchState::QtyMismatch);
    EXPECT_EQ(result.qty_diff, -100);
}

TEST_F(MatchingEngineTest, PriceMismatch) {
    auto trade = make_trade("T001", "AAPL", Side::Buy, 100, 150.0);
    auto confirm = make_confirm("T001", "AAPL", Side::Buy, 100, 155.0);
    auto result = engine.match_single(trade, confirm);
    EXPECT_EQ(result.state, MatchState::PriceMismatch);
    EXPECT_NEAR(result.price_diff, 5.0, 0.001);
}

TEST_F(MatchingEngineTest, SymbolMismatch) {
    auto trade = make_trade("T001", "AAPL");
    auto confirm = make_confirm("T001", "GOOG");
    auto result = engine.match_single(trade, confirm);
    EXPECT_NE(result.state, MatchState::Matched);
}

TEST_F(MatchingEngineTest, SideMismatch) {
    auto trade = make_trade("T001", "AAPL", Side::Buy);
    auto confirm = make_confirm("T001", "AAPL", Side::Sell);
    auto result = engine.match_single(trade, confirm);
    EXPECT_NE(result.state, MatchState::Matched);
}

TEST_F(MatchingEngineTest, BatchMatchComplete) {
    std::vector<TradeRecord> trades = {
        make_trade("T001"), make_trade("T002"), make_trade("T003")
    };
    std::vector<BrokerConfirmation> confirms = {
        make_confirm("T001"), make_confirm("T002"), make_confirm("T003")
    };

    auto results = engine.batch_match(trades, confirms);
    EXPECT_EQ(results.size(), 3);
    for (const auto& r : results) {
        EXPECT_EQ(r.state, MatchState::Matched);
    }
}

TEST_F(MatchingEngineTest, BatchMatchMissingBroker) {
    std::vector<TradeRecord> trades = {
        make_trade("T001"), make_trade("T002")
    };
    std::vector<BrokerConfirmation> confirms = {
        make_confirm("T001")
    };

    auto results = engine.batch_match(trades, confirms);
    bool found_missing = false;
    for (const auto& r : results) {
        if (r.state == MatchState::MissingBroker) found_missing = true;
    }
    EXPECT_TRUE(found_missing);
}

TEST_F(MatchingEngineTest, BatchMatchMissingInternal) {
    std::vector<TradeRecord> trades = {
        make_trade("T001")
    };
    std::vector<BrokerConfirmation> confirms = {
        make_confirm("T001"), make_confirm("T002")
    };

    auto results = engine.batch_match(trades, confirms);
    bool found_missing = false;
    for (const auto& r : results) {
        if (r.state == MatchState::MissingInternal) found_missing = true;
    }
    EXPECT_TRUE(found_missing);
}

TEST_F(MatchingEngineTest, FuzzyMatchWithinTolerance) {
    MatchingEngine fuzzy_engine(MatchingConfig{0.05, 2, false});
    auto trade = make_trade("T001", "AAPL", Side::Buy, 100, 150.0);

    BrokerConfirmation c;
    c.confirm_id = "C_FUZZY";
    c.trade_id = "T999"; // different trade_id
    c.symbol = "AAPL";
    c.side = Side::Buy;
    c.quantity = 101; // within tolerance
    c.price = 150.02; // within tolerance
    c.venue = "NYSE";
    c.timestamp_ns = trade.timestamp_ns;

    auto result = fuzzy_engine.fuzzy_match(trade, {c});
    EXPECT_EQ(result.state, MatchState::Matched);
}

TEST_F(MatchingEngineTest, AutoResolveCount) {
    std::vector<TradeRecord> trades = { make_trade("T001"), make_trade("T002") };
    std::vector<BrokerConfirmation> confirms = { make_confirm("T001"), make_confirm("T002") };

    MatchingEngine auto_engine({0.01, 1, true});
    auto_engine.batch_match(trades, confirms);
    EXPECT_EQ(auto_engine.auto_resolve_count(), 2);
}
