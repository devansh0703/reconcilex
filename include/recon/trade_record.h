#pragma once

#include <string>
#include <cstdint>
#include "recon/types.h"

namespace recon {

struct TradeRecord {
    std::string trade_id;
    uint64_t timestamp_ns = 0;
    std::string symbol;
    Side side = Side::Buy;
    int64_t quantity = 0;
    double price = 0.0;
    std::string venue;
    std::string strategy;
    std::string account;
    std::string status;
};

} // namespace recon
