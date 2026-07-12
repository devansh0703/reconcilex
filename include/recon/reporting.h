#pragma once

#include <string>
#include <vector>
#include "recon/types.h"
#include "recon/matching_engine.h"
#include "recon/exception.h"
#include "recon/settlement.h"

namespace recon {

struct ReportingConfig {
    std::string template_dir = "templates";
    std::string output_dir = "output";
};

class Reporting {
public:
    explicit Reporting(ReportingConfig config = {});

    std::string daily_reconciliation_report(const std::string& date,
                                            const std::vector<MatchResult>& matches,
                                            size_t total_internal,
                                            size_t total_confirmations) const;

    std::string exception_aging_report(const std::vector<Exception>& exceptions,
                                       const std::unordered_map<std::string, int>& aging) const;

    std::string settlement_summary_report(const std::string& date,
                                          const SettlementSummary& summary,
                                          const std::vector<SettlementRecord>& records) const;

    static void export_csv(const std::string& data, const std::string& output_path);
    static void export_json(const std::string& data, const std::string& output_path);

private:
    std::string render_template(const std::string& template_name,
                                const std::string& data_block) const;

    ReportingConfig config_;
};

} // namespace recon
