#pragma once

#include <string>
#include <cstdint>
#include "recon/types.h"

namespace recon {

struct Exception {
    uint64_t id = 0;
    MatchState match_state = MatchState::Matched;
    ExceptionSeverity severity = ExceptionSeverity::Low;
    ExceptionState state = ExceptionState::Detected;
    std::string internal_trade_id;
    std::string broker_trade_id;
    std::string symbol;
    std::string description;
    uint64_t created_at_ns = 0;
    uint64_t updated_at_ns = 0;
    std::string assignee;
    std::string notes;
    std::string resolution;
};

} // namespace recon
