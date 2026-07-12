#include <gtest/gtest.h>
#include "recon/trade_store.h"

using namespace recon;

class TradeStoreTest : public ::testing::Test {
protected:
    void SetUp() override { store = std::make_unique<TradeStore>(":memory:"); }

    std::unique_ptr<TradeStore> store;

    TradeRecord make_trade(const std::string& id, const std::string& symbol = "AAPL",
                            Side side = Side::Buy, int64_t qty = 100, double price = 150.0,
                            const std::string& venue = "NYSE") {
        TradeRecord r;
        r.trade_id = id;
        r.timestamp_ns = 1700000000000000000ULL;
        r.symbol = symbol;
        r.side = side;
        r.quantity = qty;
        r.price = price;
        r.venue = venue;
        r.strategy = "stat_arb";
        r.account = "ACC001";
        r.status = "active";
        return r;
    }

    BrokerConfirmation make_confirm(const std::string& cid, const std::string& tid,
                                     const std::string& symbol = "AAPL", Side side = Side::Buy,
                                     int64_t qty = 100, double price = 150.0,
                                     const std::string& venue = "NYSE") {
        BrokerConfirmation c;
        c.confirm_id = cid;
        c.trade_id = tid;
        c.symbol = symbol;
        c.side = side;
        c.quantity = qty;
        c.price = price;
        c.venue = venue;
        c.status = "confirmed";
        c.fees = 1.50;
        c.timestamp_ns = 1700000000000000000ULL;
        return c;
    }
};

TEST_F(TradeStoreTest, AddInternal) {
    auto r = make_trade("T001");
    EXPECT_TRUE(store->add_internal(r));
    EXPECT_EQ(store->get_internal_count(), 1);
}

TEST_F(TradeStoreTest, AddDuplicateInternalReturnsFalse) {
    auto r1 = make_trade("T001");
    auto r2 = make_trade("T001");
    EXPECT_TRUE(store->add_internal(r1));
    EXPECT_FALSE(store->add_internal(r2));
    EXPECT_EQ(store->get_internal_count(), 1);
}

TEST_F(TradeStoreTest, AddConfirmation) {
    auto c = make_confirm("C001", "T001");
    EXPECT_TRUE(store->add_confirmation(c));
    EXPECT_EQ(store->get_confirmation_count(), 1);
}

TEST_F(TradeStoreTest, QueryInternalBySymbol) {
    store->add_internal(make_trade("T001", "AAPL"));
    store->add_internal(make_trade("T002", "GOOG"));
    store->add_internal(make_trade("T003", "AAPL"));

    auto results = store->query_internal("", "", "AAPL");
    EXPECT_EQ(results.size(), 2);
}

TEST_F(TradeStoreTest, QueryInternalByVenue) {
    store->add_internal(make_trade("T001", "AAPL", Side::Buy, 100, 150.0, "NYSE"));
    store->add_internal(make_trade("T002", "AAPL", Side::Buy, 100, 150.0, "NASDAQ"));

    auto nyse = store->query_internal("", "NYSE", "");
    EXPECT_EQ(nyse.size(), 1);
    EXPECT_EQ(nyse[0].venue, "NYSE");
}

TEST_F(TradeStoreTest, QueryConfirmations) {
    store->add_confirmation(make_confirm("C001", "T001"));
    store->add_confirmation(make_confirm("C002", "T002", "GOOG"));

    auto all = store->query_confirmations();
    EXPECT_EQ(all.size(), 2);

    auto goog = store->query_confirmations("", "", "GOOG");
    EXPECT_EQ(goog.size(), 1);
}

TEST_F(TradeStoreTest, TransactionRollback) {
    store->begin_transaction();
    store->add_internal(make_trade("T001"));
    store->rollback();
    EXPECT_EQ(store->get_internal_count(), 0);
}

TEST_F(TradeStoreTest, TransactionCommit) {
    store->begin_transaction();
    store->add_internal(make_trade("T001"));
    store->commit();
    EXPECT_EQ(store->get_internal_count(), 1);
}

TEST_F(TradeStoreTest, BulkInsert) {
    store->begin_transaction();
    for (int i = 0; i < 1000; i++) {
        store->add_internal(make_trade("T" + std::to_string(i)));
    }
    store->commit();
    EXPECT_EQ(store->get_internal_count(), 1000);
}
