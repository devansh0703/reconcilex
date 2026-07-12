#pragma once

#include <string>
#include <cstdint>
#include "recon/types.h"

namespace recon {

struct BrokerConfirmation {
    std::string confirm_id;
    std::string trade_id;
    std::string symbol;
    Side side = Side::Buy;
    int64_t quantity = 0;
    double price = 0.0;
    std::string venue;
    std::string status;
    double fees = 0.0;
    uint64_t timestamp_ns = 0;
};

} // namespace recon
