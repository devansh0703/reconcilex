#pragma once

#include <vector>
#include <unordered_map>
#include <string>
#include "recon/exception.h"
#include "recon/matching_engine.h"
#include "recon/types.h"

namespace recon {

struct ExceptionFilter {
    ExceptionState state = ExceptionState::Detected;
    ExceptionSeverity severity = ExceptionSeverity::Low;
    bool filter_by_state = false;
    bool filter_by_severity = false;
    std::string assignee;
    bool filter_by_assignee = false;
};

class ExceptionWorkflow {
public:
    Exception create_exception(const MatchResult& result);

    bool acknowledge(uint64_t id);
    bool investigate(uint64_t id);
    bool resolve(uint64_t id, const std::string& notes, const std::string& resolution);
    bool escalate(uint64_t id);
    bool write_off(uint64_t id);
    bool assign(uint64_t id, const std::string& assignee);

    std::vector<Exception> get_exceptions(const ExceptionFilter& filter = {}) const;
    Exception* get_exception(uint64_t id);
    const Exception* get_exception(uint64_t id) const;

    std::unordered_map<std::string, int> get_aging_report() const;
    double get_average_resolution_time_hours() const;
    size_t total_exceptions() const { return exceptions_.size(); }

private:
    bool valid_transition(ExceptionState from, ExceptionState to) const;
    uint64_t next_id() const;

    mutable std::vector<Exception> exceptions_;
    mutable uint64_t next_id_ = 1;
};

} // namespace recon
