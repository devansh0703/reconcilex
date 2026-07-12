#include <gtest/gtest.h>
#include "recon/fix_handler.h"
#include <fix/fix44.h>

using namespace recon;

class FixHandlerTest : public ::testing::Test {
protected:
    FixHandler handler;
};

TEST_F(FixHandlerTest, ParseSimpleMessage) {
    std::string raw = "8=4.4\x01" "9=50\x01" "35=A\x01" "49=SENDER\x01" "56=TARGET\x01" "34=1\x01" "10=123\x01";
    auto msg = handler.parse(raw);
    EXPECT_EQ(msg[fix44::BeginString], "4.4");
    EXPECT_EQ(msg[fix44::MsgType], "A");
    EXPECT_EQ(msg[fix44::SenderCompID], "SENDER");
}

TEST_F(FixHandlerTest, SerializeAddsChecksum) {
    FixMessage msg;
    msg[fix44::MsgType] = "0";
    msg[fix44::SenderCompID] = "SENDER";
    msg[fix44::TargetCompID] = "TARGET";
    msg[fix44::MsgSeqNum] = "1";

    std::string serialized = handler.serialize(msg);
    EXPECT_TRUE(serialized.find("10=") != std::string::npos);
    EXPECT_TRUE(serialized.find("8=4.4") != std::string::npos);
    EXPECT_TRUE(serialized.find("35=0") != std::string::npos);
}

TEST_F(FixHandlerTest, ValidateChecksum) {
    FixMessage msg;
    msg[fix44::MsgType] = "A";
    msg[fix44::SenderCompID] = "SENDER";
    msg[fix44::TargetCompID] = "TARGET";
    msg[fix44::MsgSeqNum] = "1";

    std::string serialized = handler.serialize(msg);
    EXPECT_TRUE(handler.validate_checksum(serialized));
}

TEST_F(FixHandlerTest, InvalidChecksumDetected) {
    std::string bad = "8=4.4\x01" "9=50\x01" "35=A\x01" "49=SENDER\x01" "56=TARGET\x01" "34=1\x01" "10=999\x01";
    EXPECT_FALSE(handler.validate_checksum(bad));
}

TEST_F(FixHandlerTest, MakeLogon) {
    auto msg = handler.make_logon("SENDER", "TARGET", 1);
    auto parsed = handler.parse(msg);
    EXPECT_EQ(parsed[fix44::MsgType], fix44::MsgType_Logon);
    EXPECT_EQ(parsed[fix44::SenderCompID], "SENDER");
    EXPECT_EQ(parsed[fix44::TargetCompID], "TARGET");
    EXPECT_EQ(parsed[fix44::MsgSeqNum], "1");
}

TEST_F(FixHandlerTest, MakeHeartbeat) {
    auto msg = handler.make_heartbeat("SENDER", "TARGET", 5, "REQ001");
    auto parsed = handler.parse(msg);
    EXPECT_EQ(parsed[fix44::MsgType], fix44::MsgType_Heartbeat);
    EXPECT_EQ(parsed[112], "REQ001");
}

TEST_F(FixHandlerTest, MakeNewOrderSingle) {
    auto msg = handler.make_new_order_single("ORD001", "AAPL", 1, 100, 150.50, "SENDER", "TARGET", 3);
    auto parsed = handler.parse(msg);
    EXPECT_EQ(parsed[fix44::MsgType], fix44::MsgType_NewOrderSingle);
    EXPECT_EQ(parsed[fix44::ClOrdID], "ORD001");
    EXPECT_EQ(parsed[fix44::Symbol], "AAPL");
    EXPECT_EQ(parsed[fix44::Side], "1");
    EXPECT_EQ(parsed[fix44::OrderQty], "100");
}

TEST_F(FixHandlerTest, MakeExecutionReport) {
    auto msg = handler.make_execution_report("ORD001", "ORD001", "AAPL", 2, 100, 150.50, "2", "SENDER", "TARGET", 4);
    auto parsed = handler.parse(msg);
    EXPECT_EQ(parsed[fix44::MsgType], fix44::MsgType_ExecutionReport);
    EXPECT_EQ(parsed[fix44::ExecType], "2");
}

TEST_F(FixHandlerTest, MakeOrderCancelRequest) {
    auto msg = handler.make_order_cancel_request("ORD001", "ORD001", "AAPL", 1, 100, "SENDER", "TARGET", 5);
    auto parsed = handler.parse(msg);
    EXPECT_EQ(parsed[fix44::MsgType], fix44::MsgType_OrderCancelRequest);
}

TEST_F(FixHandlerTest, MakeTradeCaptureReport) {
    auto msg = handler.make_trade_capture_report("TR001", "AAPL", 1, 100, 150.50, "SENDER", "TARGET", 6);
    auto parsed = handler.parse(msg);
    EXPECT_EQ(parsed[fix44::MsgType], fix44::MsgType_TradeCaptureReport);
    EXPECT_EQ(parsed[fix44::TradeReportID], "TR001");
}

TEST_F(FixHandlerTest, GetMsgTypeName) {
    FixMessage logon;
    logon[fix44::MsgType] = fix44::MsgType_Logon;
    EXPECT_EQ(handler.get_msg_type_name(logon), "Logon");

    FixMessage heartbeat;
    heartbeat[fix44::MsgType] = fix44::MsgType_Heartbeat;
    EXPECT_EQ(handler.get_msg_type_name(heartbeat), "Heartbeat");

    FixMessage unknown;
    unknown[fix44::MsgType] = "ZZ";
    EXPECT_TRUE(handler.get_msg_type_name(unknown).find("Unknown") != std::string::npos);
}

TEST_F(FixHandlerTest, ParseMalformedMessage) {
    auto msg = handler.parse("not_a_fix_message");
    EXPECT_TRUE(msg.empty());
}

TEST_F(FixHandlerTest, ParseEmptyMessage) {
    auto msg = handler.parse("");
    EXPECT_TRUE(msg.empty());
}

TEST_F(FixHandlerTest, RoundTripSerialize) {
    FixMessage original;
    original[fix44::MsgType] = "D";
    original[fix44::ClOrdID] = "ORD001";
    original[fix44::Symbol] = "AAPL";
    original[fix44::Side] = "1";
    original[fix44::OrderQty] = "100";
    original[fix44::Price] = "150.5";

    std::string serialized = handler.serialize(original);
    auto parsed = handler.parse(serialized);
    EXPECT_EQ(parsed[fix44::MsgType], "D");
    EXPECT_EQ(parsed[fix44::ClOrdID], "ORD001");
    EXPECT_EQ(parsed[fix44::Symbol], "AAPL");
}
