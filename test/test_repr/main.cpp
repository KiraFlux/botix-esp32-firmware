// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Host test for Text<N> and the Representable conversions built on it.
// Run with: pio test -e native

#include <unity.h>

#include <kf/String.hpp>

#include "botix/Text.hpp"
#include "botix/protocol/Kind.hpp"
#include "botix/transport/IpEndpoint.hpp"
#include "botix/transport/Ipv4.hpp"
#include "botix/transport/Kind.hpp"

using namespace botix;

void setUp() {}

void tearDown() {}

/// @brief Render a value through the char writer exactly as console output would
template<typename T> static kf::StringView rendered(char (&buffer)[64], T const &value) {
    kf::String builder{{buffer, sizeof(buffer)}};
    builder.append(value);
    return builder.view();
}

static void text_reports_its_real_length() {
    auto const short_text = Text<32>::formatted("{}", 7);

    // The whole point of Text over Array<char, N>: no padding is carried along
    TEST_ASSERT_EQUAL_UINT32(1u, short_text.length());
    TEST_ASSERT_TRUE(short_text.view() == kf::StringView{"7"});
}

static void text_truncates_at_capacity() {
    auto const clipped = Text<4>::formatted("{}", 1234567);

    TEST_ASSERT_EQUAL_UINT32(4u, clipped.length());
}

static void ipv4_renders_dotted_quad() {
    char buffer[64];

    auto const address = transport::Ipv4::fromOctets(192, 168, 1, 42);

    TEST_ASSERT_TRUE(rendered(buffer, address) == kf::StringView{"192.168.1.42"});
}

static void ipv4_render_carries_no_padding() {
    char buffer[64];

    // A short address must not drag trailing NULs into the output
    auto const address = transport::Ipv4::fromOctets(1, 2, 3, 4);
    auto const view = rendered(buffer, address);

    TEST_ASSERT_EQUAL_UINT32(7u, view.length());
    TEST_ASSERT_TRUE(view == kf::StringView{"1.2.3.4"});
}

static void endpoint_renders_address_and_port() {
    char buffer[64];

    transport::IpEndpoint endpoint{};
    endpoint.address = transport::Ipv4::fromOctets(10, 0, 0, 1);
    endpoint.port = 14550;

    TEST_ASSERT_TRUE(rendered(buffer, endpoint) == kf::StringView{"10.0.0.1:14550"});
}

static void endpoint_emptiness_follows_address_and_port() {
    transport::IpEndpoint endpoint{};
    TEST_ASSERT_TRUE(endpoint.empty());

    endpoint.address = transport::Ipv4::fromOctets(10, 0, 0, 1);
    TEST_ASSERT_TRUE(endpoint.empty());// port still zero

    endpoint.port = 14550;
    TEST_ASSERT_FALSE(endpoint.empty());
}

static void kind_names_live_with_their_enums() {
    TEST_ASSERT_TRUE(transport::name(transport::Kind::Espnow) == kf::StringView{"espnow"});
    TEST_ASSERT_TRUE(transport::name(transport::Kind::Wifi) == kf::StringView{"wifi"});

    TEST_ASSERT_TRUE(protocol::name(protocol::Kind::Raw) == kf::StringView{"raw"});
    TEST_ASSERT_TRUE(protocol::name(protocol::Kind::Mavlink) == kf::StringView{"mavlink"});
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(text_reports_its_real_length);
    RUN_TEST(text_truncates_at_capacity);
    RUN_TEST(ipv4_renders_dotted_quad);
    RUN_TEST(ipv4_render_carries_no_padding);
    RUN_TEST(endpoint_renders_address_and_port);
    RUN_TEST(endpoint_emptiness_follows_address_and_port);
    RUN_TEST(kind_names_live_with_their_enums);

    return UNITY_END();
}
