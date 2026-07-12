#include "recon/exception_workflow.h"
#include <chrono>
#include <stdexcept>

namespace recon {

using Clock = std::chrono::system_clock;

static uint64_t now_ns() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count()
    );
}

bool ExceptionWorkflow::valid_transition(ExceptionState from, ExceptionState to) const {
    switch (from) {
        case ExceptionState::Detected:
            return to == ExceptionState::Acknowledged || to == ExceptionState::WrittenOff;
        case ExceptionState::Acknowledged:
            return to == ExceptionState::Investigating || to == ExceptionState::Escalated || to == ExceptionState::WrittenOff;
        case ExceptionState::Investigating:
            return to == ExceptionState::Resolved || to == ExceptionState::Escalated || to == ExceptionState::WrittenOff;
        case ExceptionState::Escalated:
            return to == ExceptionState::Investigating || to == ExceptionState::Resolved || to == ExceptionState::WrittenOff;
        case ExceptionState::Resolved:
            return false;
        case ExceptionState::WrittenOff:
            return false;
    }
    return false;
}

uint64_t ExceptionWorkflow::next_id() const { return next_id_++; }

Exception ExceptionWorkflow::create_exception(const MatchResult& result) {
    Exception ex;
    ex.id = next_id();
    ex.match_state = result.state;
    ex.severity = severity_from_match(result.state);
    ex.state = ExceptionState::Detected;
    ex.internal_trade_id = result.internal_trade_id;
    ex.broker_trade_id = result.broker_trade_id;
    ex.symbol = result.symbol;
    ex.description = result.details;
    ex.created_at_ns = now_ns();
    ex.updated_at_ns = ex.created_at_ns;
    exceptions_.push_back(ex);
    return ex;
}

bool ExceptionWorkflow::acknowledge(uint64_t id) {
    auto* ex = get_exception(id);
    if (!ex || !valid_transition(ex->state, ExceptionState::Acknowledged)) return false;
    ex->state = ExceptionState::Acknowledged;
    ex->updated_at_ns = now_ns();
    return true;
}

bool ExceptionWorkflow::investigate(uint64_t id) {
    auto* ex = get_exception(id);
    if (!ex || !valid_transition(ex->state, ExceptionState::Investigating)) return false;
    ex->state = ExceptionState::Investigating;
    ex->updated_at_ns = now_ns();
    return true;
}

bool ExceptionWorkflow::resolve(uint64_t id, const std::string& notes, const std::string& resolution) {
    auto* ex = get_exception(id);
    if (!ex || !valid_transition(ex->state, ExceptionState::Resolved)) return false;
    ex->state = ExceptionState::Resolved;
    ex->notes = notes;
    ex->resolution = resolution;
    ex->updated_at_ns = now_ns();
    return true;
}

bool ExceptionWorkflow::escalate(uint64_t id) {
    auto* ex = get_exception(id);
    if (!ex || !valid_transition(ex->state, ExceptionState::Escalated)) return false;
    ex->state = ExceptionState::Escalated;
    ex->updated_at_ns = now_ns();
    return true;
}

bool ExceptionWorkflow::write_off(uint64_t id) {
    auto* ex = get_exception(id);
    if (!ex || !valid_transition(ex->state, ExceptionState::WrittenOff)) return false;
    ex->state = ExceptionState::WrittenOff;
    ex->updated_at_ns = now_ns();
    return true;
}

bool ExceptionWorkflow::assign(uint64_t id, const std::string& assignee) {
    auto* ex = get_exception(id);
    if (!ex) return false;
    ex->assignee = assignee;
    ex->updated_at_ns = now_ns();
    return true;
}

Exception* ExceptionWorkflow::get_exception(uint64_t id) {
    for (auto& ex : exceptions_) {
        if (ex.id == id) return &ex;
    }
    return nullptr;
}

const Exception* ExceptionWorkflow::get_exception(uint64_t id) const {
    for (const auto& ex : exceptions_) {
        if (ex.id == id) return &ex;
    }
    return nullptr;
}

std::vector<Exception> ExceptionWorkflow::get_exceptions(const ExceptionFilter& filter) const {
    std::vector<Exception> results;
    for (const auto& ex : exceptions_) {
        if (filter.filter_by_state && ex.state != filter.state) continue;
        if (filter.filter_by_severity && ex.severity != filter.severity) continue;
        if (filter.filter_by_assignee && ex.assignee != filter.assignee) continue;
        results.push_back(ex);
    }
    return results;
}

std::unordered_map<std::string, int> ExceptionWorkflow::get_aging_report() const {
    std::unordered_map<std::string, int> aging;
    aging["0-1d"] = 0;
    aging["1-3d"] = 0;
    aging["3-7d"] = 0;
    aging["7d+"] = 0;

    uint64_t now = now_ns();
    uint64_t one_day_ns = 24ULL * 60 * 60 * 1000000000ULL;
    uint64_t three_days_ns = 3 * one_day_ns;
    uint64_t seven_days_ns = 7 * one_day_ns;

    for (const auto& ex : exceptions_) {
        if (ex.state == ExceptionState::Resolved || ex.state == ExceptionState::WrittenOff) continue;
        uint64_t age = now - ex.created_at_ns;
        if (age <= one_day_ns)       aging["0-1d"]++;
        else if (age <= three_days_ns) aging["1-3d"]++;
        else if (age <= seven_days_ns) aging["3-7d"]++;
        else                          aging["7d+"]++;
    }
    return aging;
}

double ExceptionWorkflow::get_average_resolution_time_hours() const {
    double total_hours = 0.0;
    int count = 0;
    for (const auto& ex : exceptions_) {
        if (ex.state == ExceptionState::Resolved || ex.state == ExceptionState::WrittenOff) {
            double ns = static_cast<double>(ex.updated_at_ns - ex.created_at_ns);
            total_hours += ns / (1000000000.0 * 3600.0);
            count++;
        }
    }
    return count > 0 ? total_hours / count : 0.0;
}

} // namespace recon
