// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Host test for the lexeme parsers shared by the console and config registry.
// Run with: pio test -e native

#include <unity.h>

#include "botix/Parse.hpp"

using namespace botix;

void setUp() {}

void tearDown() {}

static void integer_accepts_decimal() {
    TEST_ASSERT_EQUAL_INT64(0, parse::integer("0").unwrap());
    TEST_ASSERT_EQUAL_INT64(42, parse::integer("42").unwrap());
    TEST_ASSERT_EQUAL_INT64(-42, parse::integer("-42").unwrap());
    TEST_ASSERT_EQUAL_INT64(7, parse::integer("+7").unwrap());
    TEST_ASSERT_EQUAL_INT64(2147483647ll, parse::integer("2147483647").unwrap());
    TEST_ASSERT_EQUAL_INT64(-2147483648ll, parse::integer("-2147483648").unwrap());
}

static void integer_accepts_hex() {
    TEST_ASSERT_EQUAL_INT64(255, parse::integer("0xFF").unwrap());
    TEST_ASSERT_EQUAL_INT64(255, parse::integer("0xff").unwrap());
    TEST_ASSERT_EQUAL_INT64(16, parse::integer("0x10").unwrap());
}

static void integer_rejects_malformed() {
    TEST_ASSERT_TRUE(parse::integer("").isNone());
    TEST_ASSERT_TRUE(parse::integer("-").isNone());
    TEST_ASSERT_TRUE(parse::integer("+").isNone());
    TEST_ASSERT_TRUE(parse::integer("0x").isNone());
    TEST_ASSERT_TRUE(parse::integer("12a").isNone());
    TEST_ASSERT_TRUE(parse::integer("1.5").isNone());
    TEST_ASSERT_TRUE(parse::integer(" 12").isNone());
}

static void integer_rejects_overflow() {
    TEST_ASSERT_TRUE(parse::integer("99999999999999999999").isNone());
    TEST_ASSERT_TRUE(parse::integer("0xFFFFFFFFFFFFFFFFF").isNone());
}

static void real_accepts_decimal() {
    TEST_ASSERT_EQUAL_FLOAT(0.0f, parse::real("0").unwrap());
    TEST_ASSERT_EQUAL_FLOAT(1.5f, parse::real("1.5").unwrap());
    TEST_ASSERT_EQUAL_FLOAT(-2.25f, parse::real("-2.25").unwrap());
    TEST_ASSERT_EQUAL_FLOAT(10.0f, parse::real("10").unwrap());
    TEST_ASSERT_EQUAL_FLOAT(0.5f, parse::real(".5").unwrap());
    TEST_ASSERT_EQUAL_FLOAT(3.0f, parse::real("3.").unwrap());
}

static void real_rejects_malformed() {
    TEST_ASSERT_TRUE(parse::real("").isNone());
    TEST_ASSERT_TRUE(parse::real(".").isNone());
    TEST_ASSERT_TRUE(parse::real("1.2.3").isNone());
    TEST_ASSERT_TRUE(parse::real("abc").isNone());

    // Exponent notation is deliberately unsupported
    TEST_ASSERT_TRUE(parse::real("1e5").isNone());
}

static void ipv4_accepts_dotted_quad() {
    TEST_ASSERT_EQUAL_UINT32(0u, parse::ipv4("0.0.0.0").unwrap());
    TEST_ASSERT_EQUAL_UINT32(0xC0A8012Au, parse::ipv4("192.168.1.42").unwrap());
    TEST_ASSERT_EQUAL_UINT32(0xFFFFFFFFu, parse::ipv4("255.255.255.255").unwrap());
    TEST_ASSERT_EQUAL_UINT32(0x0A000001u, parse::ipv4("10.0.0.1").unwrap());
}

static void ipv4_rejects_malformed() {
    TEST_ASSERT_TRUE(parse::ipv4("192.168.1").isNone());
    TEST_ASSERT_TRUE(parse::ipv4("192.168.1.").isNone());
    TEST_ASSERT_TRUE(parse::ipv4("192.168.1.256").isNone());
    TEST_ASSERT_TRUE(parse::ipv4("192.168.1.1.1").isNone());
    TEST_ASSERT_TRUE(parse::ipv4("").isNone());
    TEST_ASSERT_TRUE(parse::ipv4("a.b.c.d").isNone());
    TEST_ASSERT_TRUE(parse::ipv4("192.168.1.-1").isNone());
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(integer_accepts_decimal);
    RUN_TEST(integer_accepts_hex);
    RUN_TEST(integer_rejects_malformed);
    RUN_TEST(integer_rejects_overflow);

    RUN_TEST(real_accepts_decimal);
    RUN_TEST(real_rejects_malformed);

    RUN_TEST(ipv4_accepts_dotted_quad);
    RUN_TEST(ipv4_rejects_malformed);

    return UNITY_END();
}
