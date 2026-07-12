#include <gtest/gtest.h>
#include "recon/exception_workflow.h"

using namespace recon;

class ExceptionWorkflowTest : public ::testing::Test {
protected:
    ExceptionWorkflow workflow;

    MatchResult make_match(MatchState state, const std::string& tid = "T001",
                           const std::string& symbol = "AAPL") {
        MatchResult r;
        r.state = state;
        r.internal_trade_id = tid;
        r.broker_trade_id = "B_" + tid;
        r.symbol = symbol;
        r.details = "Test mismatch";
        return r;
    }
};

TEST_F(ExceptionWorkflowTest, CreateException) {
    auto ex = workflow.create_exception(make_match(MatchState::QtyMismatch));
    EXPECT_EQ(ex.state, ExceptionState::Detected);
    EXPECT_EQ(ex.severity, ExceptionSeverity::High);
    EXPECT_EQ(workflow.total_exceptions(), 1);
}

TEST_F(ExceptionWorkflowTest, AcknowledgeTransition) {
    auto ex = workflow.create_exception(make_match(MatchState::PriceMismatch));
    EXPECT_TRUE(workflow.acknowledge(ex.id));
    auto* fetched = workflow.get_exception(ex.id);
    EXPECT_EQ(fetched->state, ExceptionState::Acknowledged);
}

TEST_F(ExceptionWorkflowTest, InvestigateTransition) {
    auto ex = workflow.create_exception(make_match(MatchState::MissingBroker));
    workflow.acknowledge(ex.id);
    EXPECT_TRUE(workflow.investigate(ex.id));
    EXPECT_EQ(workflow.get_exception(ex.id)->state, ExceptionState::Investigating);
}

TEST_F(ExceptionWorkflowTest, ResolveTransition) {
    auto ex = workflow.create_exception(make_match(MatchState::QtyMismatch));
    workflow.acknowledge(ex.id);
    workflow.investigate(ex.id);
    EXPECT_TRUE(workflow.resolve(ex.id, "Fixed qty", "Corrected in OMS"));
    auto* fetched = workflow.get_exception(ex.id);
    EXPECT_EQ(fetched->state, ExceptionState::Resolved);
    EXPECT_EQ(fetched->resolution, "Corrected in OMS");
}

TEST_F(ExceptionWorkflowTest, EscalateTransition) {
    auto ex = workflow.create_exception(make_match(MatchState::MissingInternal));
    workflow.acknowledge(ex.id);
    EXPECT_TRUE(workflow.escalate(ex.id));
    EXPECT_EQ(workflow.get_exception(ex.id)->state, ExceptionState::Escalated);
}

TEST_F(ExceptionWorkflowTest, WriteOffTransition) {
    auto ex = workflow.create_exception(make_match(MatchState::PriceMismatch));
    EXPECT_TRUE(workflow.write_off(ex.id));
    EXPECT_EQ(workflow.get_exception(ex.id)->state, ExceptionState::WrittenOff);
}

TEST_F(ExceptionWorkflowTest, InvalidTransitionResolToDetected) {
    auto ex = workflow.create_exception(make_match(MatchState::QtyMismatch));
    workflow.acknowledge(ex.id);
    workflow.investigate(ex.id);
    workflow.resolve(ex.id, "notes", "resolution");
    // Cannot go back from Resolved
    EXPECT_FALSE(workflow.acknowledge(ex.id));
}

TEST_F(ExceptionWorkflowTest, InvalidTransitionWrittenOff) {
    auto ex = workflow.create_exception(make_match(MatchState::PriceMismatch));
    workflow.write_off(ex.id);
    EXPECT_FALSE(workflow.acknowledge(ex.id));
}

TEST_F(ExceptionWorkflowTest, AssignException) {
    auto ex = workflow.create_exception(make_match(MatchState::QtyMismatch));
    EXPECT_TRUE(workflow.assign(ex.id, "john.doe"));
    EXPECT_EQ(workflow.get_exception(ex.id)->assignee, "john.doe");
}

TEST_F(ExceptionWorkflowTest, AssignNonExistent) {
    EXPECT_FALSE(workflow.assign(999, "john.doe"));
}

TEST_F(ExceptionWorkflowTest, GetExceptionNotFound) {
    EXPECT_EQ(workflow.get_exception(999), nullptr);
}

TEST_F(ExceptionWorkflowTest, AgingReport) {
    workflow.create_exception(make_match(MatchState::QtyMismatch));
    workflow.create_exception(make_match(MatchState::PriceMismatch));
    auto aging = workflow.get_aging_report();
    EXPECT_GE(aging["0-1d"], 2);
}

TEST_F(ExceptionWorkflowTest, FilterBySeverity) {
    workflow.create_exception(make_match(MatchState::QtyMismatch));     // High
    workflow.create_exception(make_match(MatchState::PriceMismatch));   // Medium
    workflow.create_exception(make_match(MatchState::MissingBroker));   // Critical

    ExceptionFilter filter;
    filter.filter_by_severity = true;
    filter.severity = ExceptionSeverity::Critical;
    auto results = workflow.get_exceptions(filter);
    EXPECT_EQ(results.size(), 1);
}

TEST_F(ExceptionWorkflowTest, FilterByAssignee) {
    auto ex1 = workflow.create_exception(make_match(MatchState::QtyMismatch));
    auto ex2 = workflow.create_exception(make_match(MatchState::PriceMismatch));
    workflow.assign(ex1.id, "alice");
    workflow.assign(ex2.id, "bob");

    ExceptionFilter filter;
    filter.filter_by_assignee = true;
    filter.assignee = "alice";
    auto results = workflow.get_exceptions(filter);
    EXPECT_EQ(results.size(), 1);
}

TEST_F(ExceptionWorkflowTest, FullLifecycle) {
    auto ex = workflow.create_exception(make_match(MatchState::QtyMismatch));
    EXPECT_TRUE(workflow.acknowledge(ex.id));
    EXPECT_TRUE(workflow.investigate(ex.id));
    EXPECT_TRUE(workflow.resolve(ex.id, "Found root cause", "Fixed"));
    auto* fetched = workflow.get_exception(ex.id);
    EXPECT_EQ(fetched->state, ExceptionState::Resolved);
    EXPECT_FALSE(fetched->resolution.empty());
}
