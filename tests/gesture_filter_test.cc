#include "gesture_filter.h"
#include "gesture_runtime_policy.h"

#include <cassert>
#include <cstring>
#include <iostream>

static void TestThresholdsAndIgnoredClasses() {
    assert(!GestureDetectionAccepted(0.499f));
    assert(GestureDetectionAccepted(0.50f));

    GestureObservation observation =
        NormalizeGestureObservation("ok", 0.849f, true);
    assert(observation.gesture == GestureClass::None);
    assert(observation.hand_present);

    observation = NormalizeGestureObservation("ok", 0.85f, true);
    assert(observation.gesture == GestureClass::Ok);
    assert(observation.hand_present);

    observation = NormalizeGestureObservation("two", 0.899f, true);
    assert(observation.gesture == GestureClass::None);
    observation = NormalizeGestureObservation("two", 0.90f, true);
    assert(observation.gesture == GestureClass::Two);
    observation = NormalizeGestureObservation("three", 0.90f, true);
    assert(observation.gesture == GestureClass::Three);

    observation = NormalizeGestureObservation("no_gesture", 0.899f, true);
    assert(observation.gesture == GestureClass::None);
    assert(observation.hand_present);
    observation = NormalizeGestureObservation("no_gesture", 0.90f, true);
    assert(observation.gesture == GestureClass::Fist);
    assert(observation.hand_present);

    observation = NormalizeGestureObservation("four", 0.85f, true);
    assert(observation.gesture == GestureClass::Four);
    assert(GestureVotesRequired(GestureClass::Four) == 3);

    for (const char *ignored : {"one", "like", "call", "dislike"}) {
        observation = NormalizeGestureObservation(ignored, 0.99f, true);
        assert(observation.gesture == GestureClass::None);
        assert(observation.hand_present);
    }

    observation = NormalizeGestureObservation("no_hand", 0.99f, true);
    assert(observation.gesture == GestureClass::None);
    assert(!observation.hand_present);
}

static void TestConsecutiveConfirmation() {
    GestureStabilityFilter filter;
    assert(filter.Process(GestureClass::Ok, true, 0).triggered == GestureClass::None);
    assert(filter.Process(GestureClass::Ok, true, 100).confirmation_count == 2);
    assert(filter.Process(GestureClass::None, true, 150).triggered == GestureClass::None);
    assert(filter.Process(GestureClass::Ok, true, 200).confirmation_count == 1);
    assert(filter.Process(GestureClass::Five, true, 300).triggered == GestureClass::None);
    assert(filter.Process(GestureClass::Ok, true, 400).confirmation_count == 1);
    assert(filter.Process(GestureClass::Ok, true, 500).confirmation_count == 2);
    GestureFilterResult result = filter.Process(GestureClass::Ok, true, 600);
    assert(result.triggered == GestureClass::Ok);
    assert(result.confirmation_count == 3);
    assert(result.votes_required == 3);
    assert(result.latched);
}

static void TestSafetyVoteRequirements() {
    GestureStabilityFilter filter;
    for (uint64_t i = 0; i < 3; ++i) {
        assert(filter.Process(GestureClass::Two, true, i * 100).triggered ==
               GestureClass::None);
    }
    GestureFilterResult door = filter.Process(GestureClass::Two, true, 300);
    assert(door.triggered == GestureClass::Two);
    assert(door.votes_required == 4);

    filter.Reset();
    for (uint64_t i = 0; i < 3; ++i) {
        assert(filter.Process(GestureClass::Fist, true, i * 100).triggered ==
               GestureClass::None);
    }
    GestureFilterResult fist = filter.Process(GestureClass::Fist, true, 300);
    assert(fist.triggered == GestureClass::Fist);
    assert(fist.votes_required == 4);
}

static void TestLatchCooldownAndDisappearance() {
    GestureStabilityFilter filter;
    filter.Process(GestureClass::Ok, true, 0);
    filter.Process(GestureClass::Ok, true, 100);
    assert(filter.Process(GestureClass::Ok, true, 200).triggered == GestureClass::Ok);

    // Holding the same gesture beyond 1.5 seconds must not trigger again.
    assert(filter.Process(GestureClass::Ok, true, 1800).latched);
    assert(filter.Process(GestureClass::Ok, true, 1900).triggered == GestureClass::None);

    // A different gesture switches directly, but still requires a complete
    // consecutive confirmation after the cooldown.
    assert(filter.Process(GestureClass::Five, true, 2000).confirmation_count == 1);
    assert(filter.Process(GestureClass::Five, true, 2100).confirmation_count == 2);
    assert(filter.Process(GestureClass::Five, true, 2200).triggered ==
           GestureClass::Five);
}

static void TestCooldownStillAppliesAfterDisappearance() {
    GestureStabilityFilter filter;
    filter.Process(GestureClass::Five, true, 0);
    filter.Process(GestureClass::Five, true, 50);
    assert(filter.Process(GestureClass::Five, true, 100).triggered == GestureClass::Five);
    assert(filter.Process(GestureClass::None, false, 200).latched);
    assert(filter.Process(GestureClass::None, false, 300).latched);
    assert(filter.Process(GestureClass::None, false, 400).latched);
    assert(filter.Process(GestureClass::None, false, 1599).latched);
    assert(!filter.Process(GestureClass::None, false, 1600).latched);
}

static void TestInactivityTimeout() {
    GestureInactivityTimer timer;
    timer.Reset(1000);
    assert(timer.RemainingSeconds(1000) == 60);
    assert(!timer.Expired(60999));
    assert(timer.Expired(61000));

    timer.Confirmed(50000);
    assert(!timer.Expired(109999));
    assert(timer.Expired(110000));
}

static void TestRuntimePolicy() {
    assert(GestureModeCanRun(kDeviceStateIdle));
    assert(!GestureModeCanRun(kDeviceStateListening));
    assert(!GestureModeCanRun(kDeviceStateSpeaking));
    assert(!GestureModeCanRun(kDeviceStateConnecting));
    assert(!GestureModeCanRun(kDeviceStateLocked));

    assert(GestureModeFramePeriodMs() == 200);
}

int main() {
    TestThresholdsAndIgnoredClasses();
    TestConsecutiveConfirmation();
    TestSafetyVoteRequirements();
    TestLatchCooldownAndDisappearance();
    TestCooldownStillAppliesAfterDisappearance();
    TestInactivityTimeout();
    TestRuntimePolicy();
    std::cout << "gesture_filter_test: all checks passed\n";
    return 0;
}
