#pragma once

namespace recon::fix44 {

// FIX 4.4 Tag definitions
constexpr int BeginString    = 8;
constexpr int BodyLength     = 9;
constexpr int MsgType        = 35;
constexpr int SenderCompID   = 49;
constexpr int TargetCompID   = 56;
constexpr int MsgSeqNum      = 34;
constexpr int SendingTime    = 52;
constexpr int CheckSum       = 10;

// Order tags
constexpr int ClOrdID        = 11;
constexpr int OrigClOrdID    = 41;
constexpr int OrderID        = 37;
constexpr int ExecID         = 17;
constexpr int ExecType       = 150;
constexpr int OrdStatus      = 39;
constexpr int Symbol         = 55;
constexpr int Side           = 54;
constexpr int OrderQty       = 38;
constexpr int Price          = 44;
constexpr int TimeInForce    = 59;
constexpr int TransactTime   = 60;

// Trade capture
constexpr int TradeReportID  = 571;
constexpr int ExecType_Report = 150;

// Message types
constexpr const char* MsgType_Logon           = "A";
constexpr const char* MsgType_Heartbeat      = "0";
constexpr const char* MsgType_TestRequest     = "1";
constexpr const char* MsgType_NewOrderSingle  = "D";
constexpr const char* MsgType_ExecutionReport = "8";
constexpr const char* MsgType_OrderCancelRequest = "F";
constexpr const char* MsgType_TradeCaptureReport = "AE";

// Exec types
constexpr const char* ExecType_New           = "0";
constexpr const char* ExecType_Filled        = "2";
constexpr const char* ExecType_Cancelled     = "4";
constexpr const char* ExecType_Rejected      = "8";

// Side values
constexpr const char* Side_Buy               = "1";
constexpr const char* Side_Sell              = "2";

// Time in force
constexpr const char* TIF_Day               = "0";
constexpr const char* TIF_GTC               = "1";
constexpr const char* TIF_IOC               = "3";

} // namespace recon::fix44
