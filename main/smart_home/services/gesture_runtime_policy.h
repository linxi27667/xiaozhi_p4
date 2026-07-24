#pragma once

#include <stdint.h>

#include "device_state.h"

constexpr uint32_t kGestureIdleFramePeriodMs = 200;

// Gesture inference and online audio are intentionally mutually exclusive.
// The service additionally checks Application::IsGestureExclusiveModeReady()
// before loading a model or starting a frame.
inline bool GestureModeCanRun(DeviceState state) {
    return state == kDeviceStateIdle;
}

inline uint32_t GestureModeFramePeriodMs() {
    return kGestureIdleFramePeriodMs;
}
