#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace recon {

enum class Side : uint8_t { Buy = 0, Sell = 1 };

inline std::string_view to_string(Side s) { return s == Side::Buy ? "Buy" : "Sell"; }
inline Side side_from_string(std::string_view s) { return s == "Buy" ? Side::Buy : Side::Sell; }

enum class MatchState : uint8_t {
    Matched = 0,
    QtyMismatch = 1,
    PriceMismatch = 2,
    MissingInternal = 3,
    MissingBroker = 4,
    Duplicate = 5
};

inline std::string_view to_string(MatchState m) {
    switch (m) {
        case MatchState::Matched:         return "Matched";
        case MatchState::QtyMismatch:     return "QtyMismatch";
        case MatchState::PriceMismatch:   return "PriceMismatch";
        case MatchState::MissingInternal: return "MissingInternal";
        case MatchState::MissingBroker:   return "MissingBroker";
        case MatchState::Duplicate:       return "Duplicate";
    }
    return "Unknown";
}

enum class ExceptionSeverity : uint8_t { Low = 0, Medium = 1, High = 2, Critical = 3 };

inline std::string_view to_string(ExceptionSeverity s) {
    switch (s) {
        case ExceptionSeverity::Low:      return "Low";
        case ExceptionSeverity::Medium:   return "Medium";
        case ExceptionSeverity::High:     return "High";
        case ExceptionSeverity::Critical: return "Critical";
    }
    return "Unknown";
}

inline ExceptionSeverity severity_from_match(MatchState m) {
    switch (m) {
        case MatchState::Matched:         return ExceptionSeverity::Low;
        case MatchState::QtyMismatch:     return ExceptionSeverity::High;
        case MatchState::PriceMismatch:   return ExceptionSeverity::Medium;
        case MatchState::MissingInternal: return ExceptionSeverity::Critical;
        case MatchState::MissingBroker:   return ExceptionSeverity::Critical;
        case MatchState::Duplicate:       return ExceptionSeverity::High;
    }
    return ExceptionSeverity::Low;
}

enum class ExceptionState : uint8_t {
    Detected = 0,
    Acknowledged = 1,
    Investigating = 2,
    Resolved = 3,
    Escalated = 4,
    WrittenOff = 5
};

inline std::string_view to_string(ExceptionState s) {
    switch (s) {
        case ExceptionState::Detected:      return "Detected";
        case ExceptionState::Acknowledged:  return "Acknowledged";
        case ExceptionState::Investigating: return "Investigating";
        case ExceptionState::Resolved:      return "Resolved";
        case ExceptionState::Escalated:     return "Escalated";
        case ExceptionState::WrittenOff:    return "WrittenOff";
    }
    return "Unknown";
}

enum class SettlementStatus : uint8_t { Pending = 0, Instructed = 1, Settled = 2, Failed = 3, BuyIn = 4 };

inline std::string_view to_string(SettlementStatus s) {
    switch (s) {
        case SettlementStatus::Pending:    return "Pending";
        case SettlementStatus::Instructed: return "Instructed";
        case SettlementStatus::Settled:    return "Settled";
        case SettlementStatus::Failed:     return "Failed";
        case SettlementStatus::BuyIn:      return "BuyIn";
    }
    return "Unknown";
}

} // namespace recon
