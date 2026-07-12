#include <gtest/gtest.h>
#include "recon/fix_handler.h"
#include <fix/fix44.h>
#include <random>
#include <string>
#include <vector>

using namespace recon;

class FuzzTest : public ::testing::Test {
protected:
    FixHandler handler;

    std::string random_string(int len, std::mt19937& rng) {
        static const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        std::uniform_int_distribution<int> dist(0, sizeof(charset) - 2);
        std::string s(len, ' ');
        for (auto& c : s) c = charset[dist(rng)];
        return s;
    }

    std::string random_fix_message(std::mt19937& rng) {
        std::uniform_int_distribution<int> len_dist(5, 200);
        int len = len_dist(rng);
        std::string msg;
        for (int i = 0; i < len; i++) {
            std::uniform_int_distribution<int> byte_dist(0, 255);
            msg += static_cast<char>(byte_dist(rng));
        }
        return msg;
    }
};

TEST_F(FuzzTest, RandomBytesDontCrash) {
    std::mt19937 rng(42);
    for (int i = 0; i < 10000; i++) {
        auto raw = random_fix_message(rng);
        auto msg = handler.parse(raw);
        // Should not crash regardless of input
        (void)msg;
    }
}

TEST_F(FuzzTest, MalformedTagValuePairs) {
    std::mt19937 rng(123);
    for (int i = 0; i < 1000; i++) {
        std::string msg;
        std::uniform_int_distribution<int> parts_dist(1, 10);
        int parts = parts_dist(rng);
        for (int j = 0; j < parts; j++) {
            // Random: sometimes valid "tag=value\x01", sometimes garbage
            if (rng() % 3 == 0) {
                msg += random_string(rng() % 20 + 1, rng);
            } else {
                std::uniform_int_distribution<int> tag_dist(1, 500);
                msg += std::to_string(tag_dist(rng)) + "=" + random_string(rng() % 10 + 1, rng) + "\x01";
            }
        }
        auto parsed = handler.parse(msg);
        // Should not crash
        (void)parsed;
    }
}

TEST_F(FuzzTest, ValidSerializedMessagesAlwaysParse) {
    std::mt19937 rng(456);
    for (int i = 0; i < 1000; i++) {
        FixMessage msg;
        msg[fix44::MsgType] = "0";
        msg[fix44::SenderCompID] = random_string(5, rng);
        msg[fix44::TargetCompID] = random_string(5, rng);
        msg[fix44::MsgSeqNum] = std::to_string(rng() % 10000);

        std::string serialized = handler.serialize(msg);
        auto parsed = handler.parse(serialized);
        EXPECT_EQ(parsed[fix44::MsgType], "0");
    }
}

TEST_F(FuzzTest, ChecksumValidationOnRandomData) {
    std::mt19937 rng(789);
    for (int i = 0; i < 1000; i++) {
        auto raw = random_fix_message(rng);
        bool valid = handler.validate_checksum(raw);
        // Should not crash, just return true/false
        (void)valid;
    }
}

TEST_F(FuzzTest, SerializeParseRoundTrip) {
    std::mt19937 rng(101);
    std::vector<std::string> msg_types = {"A", "0", "D", "8", "F", "AE"};

    for (int i = 0; i < 500; i++) {
        FixMessage msg;
        msg[fix44::MsgType] = msg_types[rng() % msg_types.size()];
        msg[fix44::SenderCompID] = random_string(8, rng);
        msg[fix44::TargetCompID] = random_string(8, rng);
        msg[fix44::MsgSeqNum] = std::to_string(rng() % 100000);
        msg[fix44::Symbol] = random_string(4, rng);

        std::string serialized = handler.serialize(msg);
        auto parsed = handler.parse(serialized);

        EXPECT_EQ(parsed[fix44::MsgType], msg[fix44::MsgType]);
        EXPECT_EQ(parsed[fix44::Symbol], msg[fix44::Symbol]);
    }
}

TEST_F(FuzzTest, EmptyAndBoundaryInputs) {
    // Empty string
    auto msg1 = handler.parse("");
    EXPECT_TRUE(msg1.empty());

    // Single SOH
    auto msg2 = handler.parse("\x01");
    EXPECT_TRUE(msg2.empty());

    // Only equals sign
    auto msg3 = handler.parse("=");
    EXPECT_TRUE(msg3.empty());

    // Very long message
    std::string long_msg(100000, 'A');
    auto msg4 = handler.parse(long_msg);
    // Should not crash
    (void)msg4;
}
