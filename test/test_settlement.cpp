#include <gtest/gtest.h>
#include "recon/settlement.h"

using namespace recon;

class SettlementTest : public ::testing::Test {
protected:
    SettlementTracker tracker;
};

TEST_F(SettlementTest, TrackSettlement) {
    tracker.track_settlement("T001", SettlementStatus::Pending, "NYSE", 15000.0);
    EXPECT_EQ(tracker.total_tracked(), 1);
}

TEST_F(SettlementTest, UpdateStatus) {
    tracker.track_settlement("T001", SettlementStatus::Pending);
    EXPECT_TRUE(tracker.update_status("T001", SettlementStatus::Instructed));
    EXPECT_EQ(tracker.get_record("T001")->status, SettlementStatus::Instructed);
}

TEST_F(SettlementTest, CannotGoBackFromSettled) {
    tracker.track_settlement("T001", SettlementStatus::Settled);
    EXPECT_FALSE(tracker.update_status("T001", SettlementStatus::Pending));
}

TEST_F(SettlementTest, FailedToBuyIn) {
    tracker.track_settlement("T001", SettlementStatus::Failed);
    EXPECT_TRUE(tracker.update_status("T001", SettlementStatus::BuyIn));
    EXPECT_EQ(tracker.get_record("T001")->status, SettlementStatus::BuyIn);
}

TEST_F(SettlementTest, CannotSettleThenFail) {
    tracker.track_settlement("T001", SettlementStatus::Settled);
    EXPECT_FALSE(tracker.update_status("T001", SettlementStatus::Failed));
}

TEST_F(SettlementTest, NonExistentTrade) {
    EXPECT_FALSE(tracker.update_status("NONEXISTENT", SettlementStatus::Pending));
    EXPECT_EQ(tracker.get_record("NONEXISTENT"), nullptr);
}

TEST_F(SettlementTest, FullLifecycle) {
    tracker.track_settlement("T001", SettlementStatus::Pending);
    EXPECT_TRUE(tracker.update_status("T001", SettlementStatus::Instructed));
    EXPECT_TRUE(tracker.update_status("T001", SettlementStatus::Settled));
    EXPECT_EQ(tracker.get_record("T001")->status, SettlementStatus::Settled);
}

TEST_F(SettlementTest, Summary) {
    tracker.track_settlement("T001", SettlementStatus::Settled);
    tracker.track_settlement("T002", SettlementStatus::Pending);
    tracker.track_settlement("T003", SettlementStatus::Failed);
    tracker.track_settlement("T004", SettlementStatus::Settled);

    auto summary = tracker.get_summary();
    EXPECT_EQ(summary.total_settled, 2);
    EXPECT_EQ(summary.total_pending, 1);
    EXPECT_EQ(summary.total_failed, 1);
    EXPECT_NEAR(summary.fail_rate, 0.25, 0.01);
}

TEST_F(SettlementTest, CheckFails) {
    uint64_t old_time = 1000000000ULL;
    uint64_t deadline = 2000000000ULL;

    tracker.track_settlement("T001", SettlementStatus::Pending);
    // Simulate old timestamp by updating record directly
    auto* rec = tracker.get_record("T001");
    rec->last_updated_ns = old_time;

    auto fails = tracker.check_fails(deadline);
    EXPECT_EQ(fails.size(), 1);
}

TEST_F(SettlementTest, GetAll) {
    tracker.track_settlement("T001", SettlementStatus::Pending);
    tracker.track_settlement("T002", SettlementStatus::Settled);
    auto all = tracker.get_all();
    EXPECT_EQ(all.size(), 2);
}
