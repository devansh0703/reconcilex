#include "recon/trade_store.h"
#include "recon/matching_engine.h"
#include "recon/exception_workflow.h"
#include "recon/settlement.h"
#include "recon/fix_handler.h"
#include "recon/reporting.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>

using namespace recon;

static void print_usage() {
    std::cout << "ReconcileX - Post-Trade Reconciliation Engine\n"
              << "Usage: reconcilex <command> [options]\n\n"
              << "Commands:\n"
              << "  match      Run matching on internal trades vs broker confirmations\n"
              << "  exceptions List or manage exceptions\n"
              << "  settlement Show settlement summary\n"
              << "  report     Generate reconciliation reports\n"
              << "  fix-parse  Parse a FIX message\n"
              << "  fix-gen    Generate a sample FIX message\n"
              << "  stats      Show trade store statistics\n"
              << "  help       Show this help\n";
}

static void cmd_stats(TradeStore& store) {
    std::cout << "Internal trades: " << store.get_internal_count() << "\n"
              << "Confirmations:   " << store.get_confirmation_count() << "\n";
}

static void cmd_match(TradeStore& store) {
    auto internals = store.query_internal();
    auto confirms = store.query_confirmations();

    MatchingEngine engine;
    auto results = engine.batch_match(internals, confirms);

    size_t matched = 0;
    for (const auto& r : results) {
        if (r.state == MatchState::Matched) matched++;
    }

    std::cout << "Matched " << matched << "/" << results.size() << " trades\n";

    // Create exceptions for non-matches
    ExceptionWorkflow workflow;
    for (const auto& r : results) {
        if (r.state != MatchState::Matched) {
            workflow.create_exception(r);
        }
    }
    auto aging = workflow.get_aging_report();
    std::cout << "Exceptions: " << workflow.total_exceptions() << "\n";
    for (const auto& [bucket, count] : aging) {
        std::cout << "  " << bucket << ": " << count << "\n";
    }
}

static void cmd_exceptions(ExceptionWorkflow& workflow) {
    auto exceptions = workflow.get_exceptions();
    std::cout << "Total exceptions: " << exceptions.size() << "\n";
    for (const auto& ex : exceptions) {
        std::cout << "  [" << ex.id << "] " << to_string(ex.match_state)
                  << " " << to_string(ex.severity) << " "
                  << to_string(ex.state) << " " << ex.symbol << "\n";
    }
}

static void cmd_settlement(SettlementTracker& tracker) {
    auto summary = tracker.get_summary();
    std::cout << "Settlement Summary:\n"
              << "  Settled:  " << summary.total_settled << "\n"
              << "  Pending:  " << summary.total_pending << "\n"
              << "  Failed:   " << summary.total_failed << "\n"
              << "  Fail rate: " << std::fixed << std::setprecision(2)
              << (summary.fail_rate * 100.0) << "%\n";
}

static void cmd_fix_parse(const std::string& raw) {
    FixHandler handler;
    auto msg = handler.parse(raw);
    std::cout << "Parsed FIX message (" << handler.get_msg_type_name(msg) << "):\n";
    for (const auto& [tag, value] : msg) {
        std::cout << "  Tag " << tag << " = " << value << "\n";
    }
    std::cout << "Checksum valid: " << (handler.validate_checksum(raw) ? "yes" : "no") << "\n";
}

static void cmd_fix_gen(const std::string& type) {
    FixHandler handler;
    std::string msg;
    if (type == "logon") {
        msg = handler.make_logon("SENDER", "TARGET", 1);
    } else if (type == "heartbeat") {
        msg = handler.make_heartbeat("SENDER", "TARGET", 2);
    } else if (type == "order") {
        msg = handler.make_new_order_single("ORD001", "AAPL", 1, 100, 150.50, "SENDER", "TARGET", 3);
    } else if (type == "exec") {
        msg = handler.make_execution_report("ORD001", "ORD001", "AAPL", 1, 100, 150.50, "2", "SENDER", "TARGET", 4);
    } else if (type == "cancel") {
        msg = handler.make_order_cancel_request("ORD001", "ORD001", "AAPL", 1, 100, "SENDER", "TARGET", 5);
    } else if (type == "trade") {
        msg = handler.make_trade_capture_report("TR001", "AAPL", 1, 100, 150.50, "SENDER", "TARGET", 6);
    } else {
        std::cerr << "Unknown type: " << type << "\n";
        return;
    }
    std::cout << msg << "\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 0;
    }

    std::string cmd = argv[1];

    try {
        TradeStore store(":memory:");
        ExceptionWorkflow workflow;
        SettlementTracker settlement;

        if (cmd == "help" || cmd == "--help") {
            print_usage();
        } else if (cmd == "stats") {
            cmd_stats(store);
        } else if (cmd == "match") {
            cmd_match(store);
        } else if (cmd == "exceptions") {
            cmd_exceptions(workflow);
        } else if (cmd == "settlement") {
            cmd_settlement(settlement);
        } else if (cmd == "fix-parse") {
            if (argc < 3) { std::cerr << "Usage: reconcilex fix-parse <fix_message>\n"; return 1; }
            cmd_fix_parse(argv[2]);
        } else if (cmd == "fix-gen") {
            if (argc < 3) { std::cerr << "Usage: reconcilex fix-gen <type>\n"; return 1; }
            cmd_fix_gen(argv[2]);
        } else if (cmd == "report") {
            Reporting reporter;
            std::vector<MatchResult> empty;
            auto html = reporter.daily_reconciliation_report("2026-01-01", empty, 0, 0);
            std::cout << html.substr(0, 200) << "... (HTML report)\n";
        } else {
            std::cerr << "Unknown command: " << cmd << "\n";
            print_usage();
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
