#pragma once

#include <cstdint>

enum class GestureClass : uint8_t {
    None = 0,
    Fist,
    Ok,
    Five,
    Two,
    Three,
    Four,
};

constexpr float kGestureDetectionThreshold = 0.50f;
constexpr float kGestureClassificationThreshold = 0.85f;
constexpr float kDoorGestureClassificationThreshold = 0.90f;
// The bundled classifier has no fist class. A sustained, very high-confidence
// no_gesture result is used only as an experimental fist proxy.
constexpr float kFistGestureClassificationThreshold = 0.90f;

struct GestureObservation {
    GestureClass gesture = GestureClass::None;
    bool hand_present = false;
};

bool GestureDetectionAccepted(float score);
GestureObservation NormalizeGestureObservation(
    const char *category, float score, bool detected_hand);
const char *GestureClassName(GestureClass gesture);
uint8_t GestureVotesRequired(GestureClass gesture);

struct GestureFilterResult {
    GestureClass candidate = GestureClass::None;
    GestureClass triggered = GestureClass::None;
    uint8_t confirmation_count = 0;
    uint8_t votes_required = 3;
    bool latched = false;
};

class GestureStabilityFilter {
public:
    GestureFilterResult Process(GestureClass gesture, bool hand_present, uint64_t now_ms);
    void Reset();

private:
    static constexpr uint8_t kAbsentFramesRequired = 3;
    static constexpr uint64_t kCooldownMs = 1500;

    GestureClass candidate_ = GestureClass::None;
    GestureClass latched_gesture_ = GestureClass::None;
    uint8_t consecutive_count_ = 0;
    bool latched_ = false;
    uint8_t absent_frames_ = 0;
    uint64_t triggered_at_ms_ = 0;

    void ClearCandidate();
};

class GestureInactivityTimer {
public:
    static constexpr uint64_t kTimeoutMs = 60000;

    void Reset(uint64_t now_ms) { deadline_ms_ = now_ms + kTimeoutMs; }
    void Confirmed(uint64_t now_ms) { Reset(now_ms); }
    bool Expired(uint64_t now_ms) const {
        return deadline_ms_ != 0 && now_ms >= deadline_ms_;
    }
    uint32_t RemainingSeconds(uint64_t now_ms) const {
        if (deadline_ms_ == 0 || now_ms >= deadline_ms_) {
            return 0;
        }
        return static_cast<uint32_t>((deadline_ms_ - now_ms + 999) / 1000);
    }

private:
    uint64_t deadline_ms_ = 0;
};
