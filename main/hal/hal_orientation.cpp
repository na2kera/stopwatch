/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "hal.h"
#include <mooncake_log.h>
#include <lvgl.h>
#include <cstdint>

static const std::string_view _tag = "HAL-Orientation";

/* -------------------------------------------------------------------------- */
/*                                Tuning knobs                                */
/* -------------------------------------------------------------------------- */
// Vertical axis of the screen is ImuData::accelY (after the HAL's X/Y swap, same as the IMU app).
// The driver returns acceleration in g. Flip kUprightSign if the detection turns out to be inverted on hardware.
static constexpr float kUprightSign = -1.0f;
// |accelY| must exceed this (in g) to count as upright / inverted; anything in between (e.g. lying flat) is ignored
static constexpr float kThresholdG = 0.6f;
// A new orientation must be stable for this long before it is applied
static constexpr uint32_t kDebounceMs = 500;
// Accelerometer polling interval
static constexpr uint32_t kPollIntervalMs = 100;
// Retry interval when no fresh sample was available (e.g. consumed by another reader)
static constexpr uint32_t kRetryIntervalMs = 20;
// Back off when the IMU keeps failing (e.g. not present)
static constexpr uint32_t kFailBackoffMs   = 1000;
static constexpr int kFailBackoffThreshold = 50;
// If no valid sample arrives for longer than this, the pending candidate is discarded so that
// unobserved time never counts towards the debounce
static constexpr uint32_t kMaxSampleGapMs = 300;
// Raw accel debug log interval
static constexpr uint32_t kDebugLogIntervalMs = 2000;

enum class Orientation { None, Upright, Inverted };

static Orientation _applied_orientation   = Orientation::Upright;
static Orientation _candidate_orientation = Orientation::None;
static uint32_t _candidate_start_tick     = 0;
static uint32_t _last_sample_tick         = 0;
static uint32_t _next_poll_tick           = 0;
static uint32_t _next_debug_log_tick      = 0;
static int _consecutive_failures          = 0;

bool Hal::isDisplayInverted() const
{
    return _applied_orientation == Orientation::Inverted;
}

static const char* orientation_name(Orientation orientation)
{
    return orientation == Orientation::Inverted ? "inverted" : "upright";
}

void Hal::updateOrientation()
{
    const uint32_t now = millis();
    if (static_cast<int32_t>(now - _next_poll_tick) < 0) {
        return;
    }

    Orientation target = Orientation::Upright;

    if (getAutoRotate()) {
        if (!updateImuData()) {
            _consecutive_failures++;
            _next_poll_tick =
                now + (_consecutive_failures >= kFailBackoffThreshold ? kFailBackoffMs : kRetryIntervalMs);
            return;
        }
        _consecutive_failures = 0;
        _next_poll_tick       = now + kPollIntervalMs;

        if (now - _last_sample_tick > kMaxSampleGapMs) {
            _candidate_orientation = Orientation::None;
        }
        _last_sample_tick = now;

        const auto& imu = getImuData();
        if (static_cast<int32_t>(now - _next_debug_log_tick) >= 0) {
            _next_debug_log_tick = now + kDebugLogIntervalMs;
            mclog::tagDebug(_tag, "accel: ({:.2f}, {:.2f}, {:.2f})", imu.accelX, imu.accelY, imu.accelZ);
        }

        const float vertical  = kUprightSign * imu.accelY;
        Orientation candidate = Orientation::None;
        if (vertical > kThresholdG) {
            candidate = Orientation::Upright;
        } else if (vertical < -kThresholdG) {
            candidate = Orientation::Inverted;
        }

        if (candidate != _candidate_orientation) {
            _candidate_orientation = candidate;
            _candidate_start_tick  = now;
        }

        if (candidate == Orientation::None || candidate == _applied_orientation ||
            now - _candidate_start_tick < kDebounceMs) {
            return;
        }

        target = candidate;
    } else {
        _next_poll_tick        = now + kPollIntervalMs;
        _candidate_orientation = Orientation::None;
    }

    if (target == _applied_orientation) {
        return;
    }

    // Defer while a button is held so that the A/B swap never happens in the middle of a press
    if (btnA.isPressed() || btnB.isPressed()) {
        return;
    }

    {
        LvglLockGuard lock;
        lv_display_t* disp = lv_display_get_default();
        if (disp == nullptr) {
            return;
        }
        lv_display_set_rotation(disp,
                                target == Orientation::Inverted ? LV_DISPLAY_ROTATION_180 : LV_DISPLAY_ROTATION_0);
    }
    _applied_orientation = target;

    const auto& imu = getImuData();
    mclog::tagInfo(_tag, "orientation -> {} ({:.2f}, {:.2f}, {:.2f})", orientation_name(target), imu.accelX, imu.accelY,
                   imu.accelZ);
}
