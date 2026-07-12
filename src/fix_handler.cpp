#include "recon/fix_handler.h"
#include <fix/fix44.h>
#include <sstream>
#include <stdexcept>
#include <numeric>
#include <algorithm>

namespace recon {

static constexpr char SOH = '\x01';

FixMessage FixHandler::parse(const std::string& raw) const {
    FixMessage msg;
    std::istringstream stream(raw);
    std::string token;

    while (std::getline(stream, token, SOH)) {
        if (token.empty()) continue;
        auto eq = token.find('=');
        if (eq == std::string::npos) continue;
        try {
            int tag = std::stoi(token.substr(0, eq));
            msg[tag] = token.substr(eq + 1);
        } catch (...) {
            continue;
        }
    }
    return msg;
}

std::string FixHandler::serialize(const FixMessage& msg) const {
    // FIX checksum: sum of all bytes from byte after BeginString+SOH through
    // the SOH terminating the last field before tag 10, mod 256.
    // BodyLength = count of bytes from BodyLength field SOH (exclusive) to CheckSum SOH (exclusive)

    // Step 1: Build body fields (everything except 8, 9, 10)
    std::ostringstream body_stream;
    for (const auto& [tag, value] : msg) {
        if (tag == fix44::BeginString || tag == fix44::BodyLength || tag == fix44::CheckSum) continue;
        body_stream << tag << "=" << value << SOH;
    }
    std::string body = body_stream.str();

    // BodyLength = bytes of: {body}{10=checksum\x01} but without the 10=CS\x01 part
    // Actually FIX spec: BodyLength = bytes from after BodyLength field's SOH to before 10= field
    int body_length = static_cast<int>(body.length());

    // Step 2: Build prefix: 8=4.4\x019=XX\x01
    std::string prefix = std::string("8=4.4") + SOH + "9=" + std::to_string(body_length) + SOH;

    // Step 3: Compute checksum over prefix + body (everything except 10=CS\x01)
    std::string for_checksum = prefix + body;
    int checksum = 0;
    for (unsigned char c : for_checksum) checksum += c;
    checksum %= 256;

    // Step 4: Assemble final message
    std::string cs_str = std::to_string(checksum);
    while (cs_str.length() < 3) cs_str = "0" + cs_str;

    return for_checksum + "10=" + cs_str + SOH;
}

std::string FixHandler::pad_checksum(int cs) const {
    std::string s = std::to_string(cs);
    while (s.length() < 3) s = "0" + s;
    return s;
}

bool FixHandler::validate_checksum(const std::string& raw) const {
    // Find the SOH before tag 10
    auto cs_pos = raw.rfind(std::string(1, SOH) + "10=");
    if (cs_pos == std::string::npos) return false;

    // Content to checksum: everything from start to and including the SOH before 10=
    std::string content = raw.substr(0, cs_pos + 1);

    int sum = 0;
    for (unsigned char c : content) sum += c;
    int expected = sum % 256;

    // Extract actual checksum value: starts at cs_pos + 4 (skip SOH, '1', '0', '=')
    auto cs_end = raw.find(SOH, cs_pos + 4);
    if (cs_end == std::string::npos) return false;
    std::string cs_str = raw.substr(cs_pos + 4, cs_end - cs_pos - 4);

    try {
        return std::stoi(cs_str) == expected;
    } catch (...) {
        return false;
    }
}

bool FixHandler::validate_checksum(const FixMessage& msg) const {
    auto it = msg.find(fix44::CheckSum);
    if (it == msg.end()) return false;

    // Rebuild the pre-checksum part
    std::string body;
    for (const auto& [tag, value] : msg) {
        if (tag == fix44::CheckSum) continue;
        body += std::to_string(tag) + "=" + value + SOH;
    }

    int sum = 0;
    for (unsigned char c : body) sum += c;
    int expected = sum % 256;

    try {
        return std::stoi(it->second) == expected;
    } catch (...) {
        return false;
    }
}

std::string FixHandler::make_logon(const std::string& sender, const std::string& target, int32_t seq) {
    FixMessage msg;
    msg[fix44::MsgType] = fix44::MsgType_Logon;
    msg[fix44::SenderCompID] = sender;
    msg[fix44::TargetCompID] = target;
    msg[fix44::MsgSeqNum] = std::to_string(seq);
    msg[fix44::SendingTime] = "20260101-00:00:00.000";
    return serialize(msg);
}

std::string FixHandler::make_heartbeat(const std::string& sender, const std::string& target,
                                        int32_t seq, const std::string& test_req_id) {
    FixMessage msg;
    msg[fix44::MsgType] = fix44::MsgType_Heartbeat;
    msg[fix44::SenderCompID] = sender;
    msg[fix44::TargetCompID] = target;
    msg[fix44::MsgSeqNum] = std::to_string(seq);
    msg[fix44::SendingTime] = "20260101-00:00:00.000";
    if (!test_req_id.empty()) msg[112] = test_req_id;
    return serialize(msg);
}

std::string FixHandler::make_new_order_single(const std::string& cl_ord_id, const std::string& symbol,
                                               int8_t side, int64_t qty, double price,
                                               const std::string& sender, const std::string& target,
                                               int32_t seq) {
    FixMessage msg;
    msg[fix44::MsgType] = fix44::MsgType_NewOrderSingle;
    msg[fix44::ClOrdID] = cl_ord_id;
    msg[fix44::Symbol] = symbol;
    msg[fix44::Side] = std::to_string(side);
    msg[fix44::OrderQty] = std::to_string(qty);
    msg[fix44::Price] = std::to_string(price);
    msg[fix44::TimeInForce] = fix44::TIF_Day;
    msg[fix44::TransactTime] = "20260101-00:00:00.000";
    msg[fix44::SenderCompID] = sender;
    msg[fix44::TargetCompID] = target;
    msg[fix44::MsgSeqNum] = std::to_string(seq);
    msg[fix44::SendingTime] = "20260101-00:00:00.000";
    return serialize(msg);
}

std::string FixHandler::make_execution_report(const std::string& order_id, const std::string& cl_ord_id,
                                               const std::string& symbol, int8_t side, int64_t qty,
                                               double price, const std::string& exec_type,
                                               const std::string& sender, const std::string& target,
                                               int32_t seq) {
    FixMessage msg;
    msg[fix44::MsgType] = fix44::MsgType_ExecutionReport;
    msg[fix44::OrderID] = order_id;
    msg[fix44::ClOrdID] = cl_ord_id;
    msg[fix44::ExecID] = "exec_" + cl_ord_id;
    msg[fix44::ExecType] = exec_type;
    msg[fix44::OrdStatus] = exec_type;
    msg[fix44::Symbol] = symbol;
    msg[fix44::Side] = std::to_string(side);
    msg[fix44::OrderQty] = std::to_string(qty);
    msg[fix44::Price] = std::to_string(price);
    msg[fix44::TransactTime] = "20260101-00:00:00.000";
    msg[fix44::SenderCompID] = sender;
    msg[fix44::TargetCompID] = target;
    msg[fix44::MsgSeqNum] = std::to_string(seq);
    msg[fix44::SendingTime] = "20260101-00:00:00.000";
    return serialize(msg);
}

std::string FixHandler::make_order_cancel_request(const std::string& order_id, const std::string& cl_ord_id,
                                                   const std::string& symbol, int8_t side, int64_t qty,
                                                   const std::string& sender, const std::string& target,
                                                   int32_t seq) {
    FixMessage msg;
    msg[fix44::MsgType] = fix44::MsgType_OrderCancelRequest;
    msg[fix44::OrigClOrdID] = cl_ord_id;
    msg[fix44::OrderID] = order_id;
    msg[fix44::Symbol] = symbol;
    msg[fix44::Side] = std::to_string(side);
    msg[fix44::OrderQty] = std::to_string(qty);
    msg[fix44::SenderCompID] = sender;
    msg[fix44::TargetCompID] = target;
    msg[fix44::MsgSeqNum] = std::to_string(seq);
    msg[fix44::SendingTime] = "20260101-00:00:00.000";
    return serialize(msg);
}

std::string FixHandler::make_trade_capture_report(const std::string& report_id, const std::string& symbol,
                                                   int8_t side, int64_t qty, double price,
                                                   const std::string& sender, const std::string& target,
                                                   int32_t seq) {
    FixMessage msg;
    msg[fix44::MsgType] = fix44::MsgType_TradeCaptureReport;
    msg[fix44::TradeReportID] = report_id;
    msg[fix44::Symbol] = symbol;
    msg[fix44::Side] = std::to_string(side);
    msg[fix44::OrderQty] = std::to_string(qty);
    msg[fix44::Price] = std::to_string(price);
    msg[fix44::TransactTime] = "20260101-00:00:00.000";
    msg[fix44::SenderCompID] = sender;
    msg[fix44::TargetCompID] = target;
    msg[fix44::MsgSeqNum] = std::to_string(seq);
    msg[fix44::SendingTime] = "20260101-00:00:00.000";
    return serialize(msg);
}

std::string FixHandler::get_msg_type_name(const FixMessage& msg) const {
    auto it = msg.find(fix44::MsgType);
    if (it == msg.end()) return "Unknown";
    const auto& t = it->second;
    if (t == fix44::MsgType_Logon)            return "Logon";
    if (t == fix44::MsgType_Heartbeat)        return "Heartbeat";
    if (t == fix44::MsgType_TestRequest)      return "TestRequest";
    if (t == fix44::MsgType_NewOrderSingle)   return "NewOrderSingle";
    if (t == fix44::MsgType_ExecutionReport)  return "ExecutionReport";
    if (t == fix44::MsgType_OrderCancelRequest) return "OrderCancelRequest";
    if (t == fix44::MsgType_TradeCaptureReport) return "TradeCaptureReport";
    return "Unknown(" + t + ")";
}

int FixHandler::compute_checksum(const std::string& msg) const {
    int sum = 0;
    for (unsigned char c : msg) sum += c;
    return sum % 256;
}

std::string FixHandler::wrap_with_checksum(const std::string& body) const {
    int cs = compute_checksum(body);
    std::string cs_str = std::to_string(cs);
    while (cs_str.length() < 3) cs_str = "0" + cs_str;
    return body + "10=" + cs_str + SOH;
}

} // namespace recon
