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
    TEST_ASSERT_EQUAL_INT64(0, Parse::integer("0").unwrap());
    TEST_ASSERT_EQUAL_INT64(42, Parse::integer("42").unwrap());
    TEST_ASSERT_EQUAL_INT64(-42, Parse::integer("-42").unwrap());
    TEST_ASSERT_EQUAL_INT64(7, Parse::integer("+7").unwrap());
    TEST_ASSERT_EQUAL_INT64(2147483647ll, Parse::integer("2147483647").unwrap());
    TEST_ASSERT_EQUAL_INT64(-2147483648ll, Parse::integer("-2147483648").unwrap());
}

static void integer_accepts_hex() {
    TEST_ASSERT_EQUAL_INT64(255, Parse::integer("0xFF").unwrap());
    TEST_ASSERT_EQUAL_INT64(255, Parse::integer("0xff").unwrap());
    TEST_ASSERT_EQUAL_INT64(16, Parse::integer("0x10").unwrap());
}

static void integer_rejects_malformed() {
    TEST_ASSERT_TRUE(Parse::integer("").isNone());
    TEST_ASSERT_TRUE(Parse::integer("-").isNone());
    TEST_ASSERT_TRUE(Parse::integer("+").isNone());
    TEST_ASSERT_TRUE(Parse::integer("0x").isNone());
    TEST_ASSERT_TRUE(Parse::integer("12a").isNone());
    TEST_ASSERT_TRUE(Parse::integer("1.5").isNone());
    TEST_ASSERT_TRUE(Parse::integer(" 12").isNone());
}

static void integer_rejects_overflow() {
    TEST_ASSERT_TRUE(Parse::integer("99999999999999999999").isNone());
    TEST_ASSERT_TRUE(Parse::integer("0xFFFFFFFFFFFFFFFFF").isNone());
}

static void real_accepts_decimal() {
    TEST_ASSERT_EQUAL_FLOAT(0.0f, Parse::real("0").unwrap());
    TEST_ASSERT_EQUAL_FLOAT(1.5f, Parse::real("1.5").unwrap());
    TEST_ASSERT_EQUAL_FLOAT(-2.25f, Parse::real("-2.25").unwrap());
    TEST_ASSERT_EQUAL_FLOAT(10.0f, Parse::real("10").unwrap());
    TEST_ASSERT_EQUAL_FLOAT(0.5f, Parse::real(".5").unwrap());
    TEST_ASSERT_EQUAL_FLOAT(3.0f, Parse::real("3.").unwrap());
}

static void real_rejects_malformed() {
    TEST_ASSERT_TRUE(Parse::real("").isNone());
    TEST_ASSERT_TRUE(Parse::real(".").isNone());
    TEST_ASSERT_TRUE(Parse::real("1.2.3").isNone());
    TEST_ASSERT_TRUE(Parse::real("abc").isNone());

    // Exponent notation is deliberately unsupported
    TEST_ASSERT_TRUE(Parse::real("1e5").isNone());
}

static void ipv4_accepts_dotted_quad() {
    TEST_ASSERT_EQUAL_UINT32(0u, Parse::ipv4("0.0.0.0").unwrap());
    TEST_ASSERT_EQUAL_UINT32(0xC0A8012Au, Parse::ipv4("192.168.1.42").unwrap());
    TEST_ASSERT_EQUAL_UINT32(0xFFFFFFFFu, Parse::ipv4("255.255.255.255").unwrap());
    TEST_ASSERT_EQUAL_UINT32(0x0A000001u, Parse::ipv4("10.0.0.1").unwrap());
}

static void ipv4_rejects_malformed() {
    TEST_ASSERT_TRUE(Parse::ipv4("192.168.1").isNone());
    TEST_ASSERT_TRUE(Parse::ipv4("192.168.1.").isNone());
    TEST_ASSERT_TRUE(Parse::ipv4("192.168.1.256").isNone());
    TEST_ASSERT_TRUE(Parse::ipv4("192.168.1.1.1").isNone());
    TEST_ASSERT_TRUE(Parse::ipv4("").isNone());
    TEST_ASSERT_TRUE(Parse::ipv4("a.b.c.d").isNone());
    TEST_ASSERT_TRUE(Parse::ipv4("192.168.1.-1").isNone());
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
