#include "gesture_filter.h"

#include <algorithm>
#include <cstring>

bool GestureDetectionAccepted(float score) {
    return score >= kGestureDetectionThreshold;
}

GestureObservation NormalizeGestureObservation(
    const char *category, float score, bool detected_hand) {
    GestureObservation observation;
    if (!detected_hand) {
        return observation;
    }

    observation.hand_present = true;
    if (category == nullptr) {
        return observation;
    }
    if (std::strcmp(category, "no_hand") == 0) {
        observation.hand_present = false;
        return observation;
    }
    if (std::strcmp(category, "no_gesture") == 0) {
        if (score >= kFistGestureClassificationThreshold) {
            observation.gesture = GestureClass::Fist;
        }
        return observation;
    }

    if (std::strcmp(category, "two") == 0) {
        if (score >= kDoorGestureClassificationThreshold) {
            observation.gesture = GestureClass::Two;
        }
    } else if (std::strcmp(category, "three") == 0) {
        if (score >= kDoorGestureClassificationThreshold) {
            observation.gesture = GestureClass::Three;
        }
    } else if (score < kGestureClassificationThreshold) {
        return observation;
    } else if (std::strcmp(category, "ok") == 0) {
        observation.gesture = GestureClass::Ok;
    } else if (std::strcmp(category, "five") == 0) {
        observation.gesture = GestureClass::Five;
    } else if (std::strcmp(category, "four") == 0) {
        observation.gesture = GestureClass::Four;
    }
    return observation;
}

const char *GestureClassName(GestureClass gesture) {
    switch (gesture) {
        case GestureClass::Fist:
            return "fist";
        case GestureClass::Ok:
            return "ok";
        case GestureClass::Five:
            return "five";
        case GestureClass::Two:
            return "two";
        case GestureClass::Three:
            return "three";
        case GestureClass::Four:
            return "four";
        default:
            return "";
    }
}

uint8_t GestureVotesRequired(GestureClass gesture) {
    switch (gesture) {
        case GestureClass::Fist:
            return 4;
        case GestureClass::Two:
        case GestureClass::Three:
            return 4;
        default:
            return 3;
    }
}

void GestureStabilityFilter::Reset() {
    ClearCandidate();
    latched_gesture_ = GestureClass::None;
    latched_ = false;
    absent_frames_ = 0;
    triggered_at_ms_ = 0;
}

void GestureStabilityFilter::ClearCandidate() {
    candidate_ = GestureClass::None;
    consecutive_count_ = 0;
}

GestureFilterResult GestureStabilityFilter::Process(
    GestureClass gesture, bool hand_present, uint64_t now_ms) {
    GestureFilterResult result;

    if (latched_) {
        absent_frames_ = hand_present ? 0 : std::min<uint8_t>(
            static_cast<uint8_t>(absent_frames_ + 1), kAbsentFramesRequired);
        const bool cooldown_elapsed = now_ms - triggered_at_ms_ >= kCooldownMs;
        if (!hand_present) {
            ClearCandidate();
            if (cooldown_elapsed && absent_frames_ >= kAbsentFramesRequired) {
                Reset();
            } else {
                result.latched = true;
                return result;
            }
        } else if (gesture == GestureClass::None ||
                   gesture == latched_gesture_) {
            ClearCandidate();
            result.candidate = gesture;
            result.votes_required = GestureVotesRequired(gesture);
            result.latched = true;
            return result;
        } else if (!cooldown_elapsed) {
            ClearCandidate();
            result.candidate = gesture;
            result.votes_required = GestureVotesRequired(gesture);
            result.latched = true;
            return result;
        } else {
            // A distinctly different stable gesture may switch actions after
            // the cooldown without requiring the user to leave the frame.
            ClearCandidate();
            latched_ = false;
            absent_frames_ = 0;
        }
    }

    if (gesture == GestureClass::None) {
        ClearCandidate();
        return result;
    }

    if (gesture != candidate_) {
        candidate_ = gesture;
        consecutive_count_ = 1;
    } else {
        consecutive_count_ = std::min<uint8_t>(
            static_cast<uint8_t>(consecutive_count_ + 1),
            GestureVotesRequired(gesture));
    }

    result.candidate = candidate_;
    result.confirmation_count = consecutive_count_;
    result.votes_required = GestureVotesRequired(candidate_);
    if (consecutive_count_ >= result.votes_required) {
        const GestureClass triggered = candidate_;
        latched_ = true;
        latched_gesture_ = triggered;
        triggered_at_ms_ = now_ms;
        absent_frames_ = 0;
        ClearCandidate();
        result.candidate = triggered;
        result.triggered = triggered;
        result.latched = true;
    }
    return result;
}
