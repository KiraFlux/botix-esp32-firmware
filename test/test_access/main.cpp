// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Host test for offset-based configuration field access.
// Run with: pio test -e native

#include <unity.h>

#include "botix/config/Access.hpp"

using namespace botix::config;

/// @brief Stand-in for a real config blob
/// @note Widths are deliberately mixed and guarded on both ends, so a wrong
///       offset or size spills into a neighbour and is caught.
struct Sample {
    kf::u8 guard_front;
    bool flag;
    kf::i8 small_signed;
    kf::u16 port;
    kf::i32 wide_signed;
    kf::f32 gain;
    kf::f64 scale;
    kf::u32 ipv4;
    kf::u8 mode;
    char text[8];
    kf::u8 guard_back;
};

enum class Mode : kf::u8 { Off = 0,
                           Slow = 1,
                           Fast = 2 };

static constexpr EnumOption mode_options[]{
    {"off", static_cast<kf::usize>(Mode::Off)},
    {"slow", static_cast<kf::usize>(Mode::Slow)},
    {"fast", static_cast<kf::usize>(Mode::Fast)},
};

static constexpr Field fields[]{
    {"flag", offsetof(Sample, flag), sizeof(bool), Kind::Boolean, false, {}, kf::none, kf::none},
    {"small", offsetof(Sample, small_signed), sizeof(kf::i8), Kind::Signed, false, {}, kf::none, kf::none},
    {"port", offsetof(Sample, port), sizeof(kf::u16), Kind::Unsigned, false, {}, kf::none, kf::none},
    {"wide", offsetof(Sample, wide_signed), sizeof(kf::i32), Kind::Signed, false, {}, kf::none, kf::none},
    {"gain", offsetof(Sample, gain), sizeof(kf::f32), Kind::Real, false, {}, kf::some(0.0), kf::some(10.0)},
    {"scale", offsetof(Sample, scale), sizeof(kf::f64), Kind::Real, false, {}, kf::none, kf::none},
    {"ip", offsetof(Sample, ipv4), sizeof(kf::u32), Kind::Ipv4, false, {}, kf::none, kf::none},
    {"mode", offsetof(Sample, mode), sizeof(kf::u8), Kind::Enumerated, false, {mode_options}, kf::none, kf::none},
    {"text", offsetof(Sample, text), sizeof(Sample::text), Kind::Text, false, {}, kf::none, kf::none},
};

static Sample sample{};

static Section section{
    .name = "sample",
    .bytes = {reinterpret_cast<kf::u8 *>(&sample), sizeof(Sample)},
    .fields = {fields},
};

static Field const &field(kf::StringView path) {
    return section.find(path).unwrap();
}

void setUp() {
    sample = Sample{};
    sample.guard_front = 0xAB;
    sample.guard_back = 0xCD;
}

void tearDown() {
    // Every case must stay inside its own field
    TEST_ASSERT_EQUAL_UINT8(0xAB, sample.guard_front);
    TEST_ASSERT_EQUAL_UINT8(0xCD, sample.guard_back);
}

static void boolean_round_trip() {
    TEST_ASSERT_TRUE(Access::set(section, field("flag"), "true") == SetStatus::Ok);
    TEST_ASSERT_TRUE(Access::readBoolean(section, field("flag")));

    TEST_ASSERT_TRUE(Access::set(section, field("flag"), "no") == SetStatus::Ok);
    TEST_ASSERT_FALSE(Access::readBoolean(section, field("flag")));

    TEST_ASSERT_TRUE(Access::set(section, field("flag"), "maybe") == SetStatus::Malformed);
}

static void signed_respects_width() {
    TEST_ASSERT_TRUE(Access::set(section, field("small"), "-128") == SetStatus::Ok);
    TEST_ASSERT_EQUAL_INT64(-128, Access::readSigned(section, field("small")));

    TEST_ASSERT_TRUE(Access::set(section, field("small"), "127") == SetStatus::Ok);
    TEST_ASSERT_EQUAL_INT64(127, Access::readSigned(section, field("small")));

    // Rejected values must leave the stored one alone
    TEST_ASSERT_TRUE(Access::set(section, field("small"), "128") == SetStatus::OutOfRange);
    TEST_ASSERT_EQUAL_INT64(127, Access::readSigned(section, field("small")));

    TEST_ASSERT_TRUE(Access::set(section, field("wide"), "-2000000000") == SetStatus::Ok);
    TEST_ASSERT_EQUAL_INT64(-2000000000ll, Access::readSigned(section, field("wide")));
}

static void unsigned_rejects_negative_and_overflow() {
    TEST_ASSERT_TRUE(Access::set(section, field("port"), "14550") == SetStatus::Ok);
    TEST_ASSERT_EQUAL_UINT64(14550u, Access::readUnsigned(section, field("port")));

    TEST_ASSERT_TRUE(Access::set(section, field("port"), "65536") == SetStatus::OutOfRange);
    TEST_ASSERT_TRUE(Access::set(section, field("port"), "-1") == SetStatus::OutOfRange);
    TEST_ASSERT_EQUAL_UINT64(14550u, Access::readUnsigned(section, field("port")));
}

static void real_honours_declared_bounds() {
    TEST_ASSERT_TRUE(Access::set(section, field("gain"), "2.5") == SetStatus::Ok);
    TEST_ASSERT_EQUAL_DOUBLE(2.5, Access::readReal(section, field("gain")));

    TEST_ASSERT_TRUE(Access::set(section, field("gain"), "11") == SetStatus::OutOfRange);
    TEST_ASSERT_TRUE(Access::set(section, field("gain"), "-0.5") == SetStatus::OutOfRange);
    TEST_ASSERT_EQUAL_DOUBLE(2.5, Access::readReal(section, field("gain")));
}

static void real_uses_field_width() {
    // A 64-bit field must be stored wide, not truncated through f32
    TEST_ASSERT_TRUE(Access::set(section, field("scale"), "0.25") == SetStatus::Ok);
    TEST_ASSERT_EQUAL_DOUBLE(0.25, Access::readReal(section, field("scale")));
    TEST_ASSERT_EQUAL_DOUBLE(0.25, sample.scale);
}

static void ipv4_round_trip() {
    TEST_ASSERT_TRUE(Access::set(section, field("ip"), "192.168.1.42") == SetStatus::Ok);
    TEST_ASSERT_EQUAL_UINT32(0xC0A8012Au, Access::readIpv4(section, field("ip")).value);

    TEST_ASSERT_TRUE(Access::set(section, field("ip"), "192.168.1.999") == SetStatus::Malformed);
    TEST_ASSERT_EQUAL_UINT32(0xC0A8012Au, Access::readIpv4(section, field("ip")).value);
}

static void enumerated_round_trip() {
    TEST_ASSERT_TRUE(Access::set(section, field("mode"), "fast") == SetStatus::Ok);
    TEST_ASSERT_EQUAL_UINT64(2u, Access::readUnsigned(section, field("mode")));
    TEST_ASSERT_TRUE(Access::readOptionName(section, field("mode")) == kf::StringView{"fast"});

    TEST_ASSERT_TRUE(Access::set(section, field("mode"), "turbo") == SetStatus::UnknownOption);
    TEST_ASSERT_EQUAL_UINT64(2u, Access::readUnsigned(section, field("mode")));
}

static void text_respects_terminator_budget() {
    TEST_ASSERT_TRUE(Access::set(section, field("text"), "abc") == SetStatus::Ok);
    TEST_ASSERT_TRUE(Access::readText(section, field("text")) == kf::StringView{"abc"});

    // 7 characters plus the terminator exactly fills an 8-byte field
    TEST_ASSERT_TRUE(Access::set(section, field("text"), "1234567") == SetStatus::Ok);
    TEST_ASSERT_TRUE(Access::readText(section, field("text")) == kf::StringView{"1234567"});

    TEST_ASSERT_TRUE(Access::set(section, field("text"), "12345678") == SetStatus::TooLong);
    TEST_ASSERT_TRUE(Access::readText(section, field("text")) == kf::StringView{"1234567"});
}

static void text_strips_quotes_to_allow_empty() {
    TEST_ASSERT_TRUE(Access::set(section, field("text"), "\"xy\"") == SetStatus::Ok);
    TEST_ASSERT_TRUE(Access::readText(section, field("text")) == kf::StringView{"xy"});

    // Quoting is the only way to express an empty value: the console tokenizer
    // discards empty lexemes before they reach here.
    TEST_ASSERT_TRUE(Access::set(section, field("text"), "\"\"") == SetStatus::Ok);
    TEST_ASSERT_TRUE(Access::readText(section, field("text")).empty());

    TEST_ASSERT_TRUE(Access::set(section, field("text"), "''") == SetStatus::Ok);
    TEST_ASSERT_TRUE(Access::readText(section, field("text")).empty());
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(boolean_round_trip);
    RUN_TEST(signed_respects_width);
    RUN_TEST(unsigned_rejects_negative_and_overflow);
    RUN_TEST(real_honours_declared_bounds);
    RUN_TEST(real_uses_field_width);
    RUN_TEST(ipv4_round_trip);
    RUN_TEST(enumerated_round_trip);
    RUN_TEST(text_respects_terminator_budget);
    RUN_TEST(text_strips_quotes_to_allow_empty);

    return UNITY_END();
}
