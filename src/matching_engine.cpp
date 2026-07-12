#include "recon/matching_engine.h"
#include <unordered_map>
#include <cmath>
#include <sstream>
#include <algorithm>

namespace recon {

MatchingEngine::MatchingEngine(MatchingConfig config) : config_(std::move(config)) {}

MatchResult MatchingEngine::match_single(const TradeRecord& internal,
                                         const BrokerConfirmation& confirm) const {
    MatchResult result;
    result.internal_trade_id = internal.trade_id;
    result.broker_trade_id = confirm.trade_id;
    result.symbol = internal.symbol;

    std::ostringstream details;

    bool has_mismatch = false;

    if (internal.symbol != confirm.symbol) {
        details << "Symbol mismatch: " << internal.symbol << " vs " << confirm.symbol << "; ";
        has_mismatch = true;
    }

    if (internal.side != confirm.side) {
        details << "Side mismatch; ";
        has_mismatch = true;
    }

    int64_t qty_diff = internal.quantity - confirm.quantity;
    result.qty_diff = qty_diff;
    if (std::abs(qty_diff) > config_.qty_tolerance) {
        details << "Qty mismatch: " << internal.quantity << " vs " << confirm.quantity
                << " (diff=" << qty_diff << "); ";
        has_mismatch = true;
    }

    double price_diff = std::abs(internal.price - confirm.price);
    result.price_diff = price_diff;
    if (price_diff > config_.price_tolerance) {
        details << "Price mismatch: " << internal.price << " vs " << confirm.price
                << " (diff=" << price_diff << "); ";
        has_mismatch = true;
    }

    if (!has_mismatch) {
        result.state = MatchState::Matched;
        result.details = "Exact match";
    } else if (std::abs(qty_diff) > config_.qty_tolerance) {
        result.state = MatchState::QtyMismatch;
        result.details = details.str();
    } else {
        result.state = MatchState::PriceMismatch;
        result.details = details.str();
    }

    return result;
}

MatchResult MatchingEngine::fuzzy_match(const TradeRecord& internal,
                                         const std::vector<BrokerConfirmation>& candidates) const {
    for (const auto& c : candidates) {
        if (internal.symbol == c.symbol && internal.side == c.side) {
            int64_t qty_diff = std::abs(internal.quantity - c.quantity);
            double price_diff = std::abs(internal.price - c.price);
            if (qty_diff <= config_.qty_tolerance && price_diff <= config_.price_tolerance) {
                auto result = match_single(internal, c);
                if (result.state == MatchState::Matched || result.state == MatchState::PriceMismatch) {
                    return result;
                }
            }
        }
    }

    MatchResult result;
    result.internal_trade_id = internal.trade_id;
    result.symbol = internal.symbol;
    result.state = MatchState::MissingBroker;
    result.details = "No matching broker confirmation found";
    return result;
}

std::vector<MatchResult> MatchingEngine::batch_match(
    const std::vector<TradeRecord>& internals,
    const std::vector<BrokerConfirmation>& confirms) const {

    std::vector<MatchResult> results;

    // Build hash map of confirmations by trade_id
    std::unordered_map<std::string, std::vector<const BrokerConfirmation*>> confirm_map;
    for (const auto& c : confirms) {
        confirm_map[c.trade_id].push_back(&c);
    }

    std::unordered_map<std::string, bool> matched_confirms;

    // Match internal trades against confirmations
    for (const auto& internal : internals) {
        auto it = confirm_map.find(internal.trade_id);
        if (it != confirm_map.end() && !it->second.empty()) {
            const BrokerConfirmation* best = it->second[0];
            auto result = match_single(internal, *best);

            // Check for duplicates
            if (it->second.size() > 1) {
                result.state = MatchState::Duplicate;
                result.details = "Multiple confirmations for trade_id: " + internal.trade_id;
            }

            results.push_back(std::move(result));
            matched_confirms[internal.trade_id] = true;

            if (config_.auto_resolve && result.state == MatchState::Matched) {
                auto_resolved_++;
            }
        } else {
            // Fuzzy match attempt
            std::vector<const BrokerConfirmation*> unmatched;
            for (const auto& c : confirms) {
                if (matched_confirms.find(c.trade_id) == matched_confirms.end()) {
                    unmatched.push_back(&c);
                }
            }

            std::vector<BrokerConfirmation> unmatched_copy;
            for (auto* p : unmatched) unmatched_copy.push_back(*p);

            auto result = fuzzy_match(internal, unmatched_copy);
            results.push_back(std::move(result));
        }
    }

    // Find missing internal (confirmations without internal trades)
    for (const auto& c : confirms) {
        if (matched_confirms.find(c.trade_id) == matched_confirms.end()) {
            MatchResult result;
            result.state = MatchState::MissingInternal;
            result.broker_trade_id = c.trade_id;
            result.symbol = c.symbol;
            result.details = "No internal trade for broker confirmation: " + c.confirm_id;
            results.push_back(std::move(result));
        }
    }

    return results;
}

} // namespace recon
