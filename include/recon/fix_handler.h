#pragma once

#include <map>
#include <string>
#include <cstdint>

namespace recon {

using FixMessage = std::map<int, std::string>;

struct FixSession {
    std::string sender_comp_id;
    std::string target_comp_id;
    int32_t outgoing_seq = 1;
    int32_t incoming_seq = 1;
    bool logged_in = false;
};

class FixHandler {
public:
    FixMessage parse(const std::string& raw) const;
    std::string serialize(const FixMessage& msg) const;

    bool validate_checksum(const std::string& raw) const;
    bool validate_checksum(const FixMessage& msg) const;

    std::string make_logon(const std::string& sender, const std::string& target, int32_t seq);
    std::string make_heartbeat(const std::string& sender, const std::string& target, int32_t seq,
                               const std::string& test_req_id = "");

    std::string make_new_order_single(const std::string& cl_ord_id, const std::string& symbol,
                                       int8_t side, int64_t qty, double price,
                                       const std::string& sender, const std::string& target,
                                       int32_t seq);

    std::string make_execution_report(const std::string& order_id, const std::string& cl_ord_id,
                                       const std::string& symbol, int8_t side, int64_t qty,
                                       double price, const std::string& exec_type,
                                       const std::string& sender, const std::string& target,
                                       int32_t seq);

    std::string make_order_cancel_request(const std::string& order_id, const std::string& cl_ord_id,
                                           const std::string& symbol, int8_t side, int64_t qty,
                                           const std::string& sender, const std::string& target,
                                           int32_t seq);

    std::string make_trade_capture_report(const std::string& report_id, const std::string& symbol,
                                           int8_t side, int64_t qty, double price,
                                           const std::string& sender, const std::string& target,
                                           int32_t seq);

    std::string get_msg_type_name(const FixMessage& msg) const;

    FixSession& session() { return session_; }
    const FixSession& session() const { return session_; }

private:
    int compute_checksum(const std::string& msg) const;
    std::string wrap_with_checksum(const std::string& body) const;
    std::string pad_checksum(int cs) const;
    FixSession session_;
};

} // namespace recon
