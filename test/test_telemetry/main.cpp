// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Host test for incoming telemetry ageing.
// Run with: pio test -e native

#include <unity.h>

#include "botix/IncomingTelemetry.hpp"

using namespace botix;

void setUp() {}

void tearDown() {}

static IncomingTelemetry::ControlInput const sample{
    .r_axis = 1,
    .z_axis = 2,
    .y_axis = 3,
    .x_axis = 4,
};

static void age_grows_with_time() {
    IncomingTelemetry::ControlInputEntry entry{};

    entry.update(sample, 1'000);

    TEST_ASSERT_EQUAL_UINT32(0u, entry.age(1'000));
    TEST_ASSERT_EQUAL_UINT32(50u, entry.age(1'050));
    TEST_ASSERT_EQUAL_UINT32(5'000u, entry.age(6'000));
}

static void age_saturates_when_stamped_ahead() {
    IncomingTelemetry::ControlInputEntry entry{};

    // A polled transport receives inside the same loop iteration whose timestamp
    // was taken beforehand, so the entry can be stamped one tick into the future.
    entry.update(sample, 1'001);

    // Subtracting unsigned would wrap to 0xFFFFFFFF and make the freshest
    // possible input look about 49 days stale, which holds the motors at zero.
    TEST_ASSERT_EQUAL_UINT32(0u, entry.age(1'000));
    TEST_ASSERT_EQUAL_UINT32(0u, entry.age(1'001));
    TEST_ASSERT_EQUAL_UINT32(9u, entry.age(1'010));
}

static void fresh_input_stays_inside_the_mixer_tolerance() {
    constexpr kf::units::Milliseconds tolerance{100};

    IncomingTelemetry::ControlInputEntry entry{};
    entry.update(sample, 5'000);

    // Anything the mixer would accept
    TEST_ASSERT_TRUE(entry.age(5'000) <= tolerance);
    TEST_ASSERT_TRUE(entry.age(5'099) <= tolerance);

    // And the point where it must give up and stop the motors
    TEST_ASSERT_TRUE(entry.age(5'200) > tolerance);

    // A future stamp must never be mistaken for a timeout
    entry.update(sample, 6'001);
    TEST_ASSERT_TRUE(entry.age(6'000) <= tolerance);
}

static void update_replaces_the_value() {
    IncomingTelemetry::ControlInputEntry entry{};

    entry.update(sample, 1'000);
    TEST_ASSERT_EQUAL_INT16(2, entry.value().z_axis);

    IncomingTelemetry::ControlInput const other{
        .r_axis = 0,
        .z_axis = 700,
        .y_axis = 0,
        .x_axis = 0,
    };

    entry.update(other, 1'100);
    TEST_ASSERT_EQUAL_INT16(700, entry.value().z_axis);
    TEST_ASSERT_EQUAL_UINT32(0u, entry.age(1'100));
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(age_grows_with_time);
    RUN_TEST(age_saturates_when_stamped_ahead);
    RUN_TEST(fresh_input_stays_inside_the_mixer_tolerance);
    RUN_TEST(update_replaces_the_value);

    return UNITY_END();
}
