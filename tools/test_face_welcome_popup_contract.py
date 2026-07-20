#!/usr/bin/env python3
"""Regression checks for face-only welcome popup routing."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
APPLICATION = ROOT / "main/application.cc"
EVENT_HEADER = ROOT / "main/smart_home/ui/core/ui_events.h"
EVENT_SOURCE = ROOT / "main/smart_home/ui/core/ui_events.c"
POPUP_SOURCE = ROOT / "main/smart_home/ui/services/ui_welcome_popup.c"
SCENE_PAGE = ROOT / "main/smart_home/ui/pages/page_scene.c"


class FaceWelcomePopupContractTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.application = APPLICATION.read_text(encoding="utf-8")
        cls.event_header = EVENT_HEADER.read_text(encoding="utf-8")
        cls.event_source = EVENT_SOURCE.read_text(encoding="utf-8")
        cls.popup = POPUP_SOURCE.read_text(encoding="utf-8")
        cls.scene_page = SCENE_PAGE.read_text(encoding="utf-8")

    def test_face_unlocked_event_is_registered(self):
        self.assertIn("UI_EVENT_FACE_UNLOCKED", self.event_header)
        self.assertIn(
            '[UI_EVENT_FACE_UNLOCKED]          = "FACE_UNLOCKED"',
            self.event_source,
        )

    def test_welcome_popup_only_subscribes_to_face_unlock(self):
        self.assertIn(
            "ui_event_subscribe(UI_EVENT_FACE_UNLOCKED, on_face_unlocked, NULL)",
            self.popup,
        )
        self.assertNotIn("UI_EVENT_SCENE_CHANGED", self.popup)
        self.assertNotIn("IOT_SCENE_HOME", self.popup)

    def test_face_path_publishes_after_successful_unlock(self):
        unlock_call = self.application.index("app.OnLoginSuccess();")
        face_event = self.application.index(
            "ui_event_publish(UI_EVENT_FACE_UNLOCKED);", unlock_call
        )
        self.assertGreater(face_event, unlock_call)
        self.assertEqual(
            self.application.count("ui_event_publish(UI_EVENT_FACE_UNLOCKED);"), 1
        )

    def test_manual_scene_page_does_not_publish_face_unlock(self):
        self.assertIn("mqtt_send_scene(item->id);", self.scene_page)
        self.assertNotIn("UI_EVENT_FACE_UNLOCKED", self.scene_page)


if __name__ == "__main__":
    unittest.main(verbosity=2)
