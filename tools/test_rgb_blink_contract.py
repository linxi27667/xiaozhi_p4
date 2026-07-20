#!/usr/bin/env python3
"""Regression checks for P4-driven RGB blinking."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
PROTOCOL = ROOT / "shared/mqtt_iot_protocol.h"
MCP_SOURCE = ROOT / "main/smart_home/mcp/smart_home_mcp_tool.cc"
AMBIENT_SOURCE = (
    ROOT
    / "slave/xiaozhi_slave_Secondfloor/main/APP/Src/app_ambient_light.c"
)
MQTT_SOURCE = ROOT / "main/smart_home/services/xiaozhi_mqtt.cc"
TASK_SOURCE = ROOT / "main/smart_home/tasks/smart_home_tasks.cc"


class RgbBlinkContractTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.protocol = PROTOCOL.read_text(encoding="utf-8")
        cls.mcp = MCP_SOURCE.read_text(encoding="utf-8")
        cls.ambient = AMBIENT_SOURCE.read_text(encoding="utf-8")
        cls.mqtt = MQTT_SOURCE.read_text(encoding="utf-8")
        cls.tasks = TASK_SOURCE.read_text(encoding="utf-8")

    def test_protocol_does_not_add_a_blink_effect(self):
        self.assertNotIn("IOT_LIGHT_EFFECT_BLINK", self.protocol)
        self.assertNotIn("IOT_LIGHT_EFFECT_BLINK", self.ambient)

    def test_ai_tool_routes_blink_to_p4_sequence(self):
        self.assertIn('Property("effect_name", kPropertyTypeString, std::string())', self.mcp)
        self.assertIn('Property("effect", kPropertyTypeInteger, 0, 0, 3)', self.mcp)
        self.assertIn('effect_name == "blink" || effect_name == "闪烁"', self.mcp)
        self.assertIn("return mqtt_start_rgb_blink", self.mcp)
        self.assertIn(
            'Property("blink_count", kPropertyTypeInteger, 3, 1, 10)', self.mcp
        )

    def test_ai_tool_exposes_every_light_effect_by_name(self):
        expected_mappings = {
            '"static", IOT_LIGHT_EFFECT_STATIC',
            '"常亮", IOT_LIGHT_EFFECT_STATIC',
            '"breathe", IOT_LIGHT_EFFECT_BREATHE',
            '"呼吸", IOT_LIGHT_EFFECT_BREATHE',
            '"rainbow", IOT_LIGHT_EFFECT_RAINBOW',
            '"彩虹", IOT_LIGHT_EFFECT_RAINBOW',
            '"warning", IOT_LIGHT_EFFECT_WARNING',
            '"警示", IOT_LIGHT_EFFECT_WARNING',
        }
        for mapping in expected_mappings:
            with self.subTest(mapping=mapping):
                self.assertIn(mapping, self.mcp)

    def test_effect_name_takes_priority_over_legacy_warning_fallback(self):
        name_branch = self.mcp.index("if (!effect_name.empty() && !blink)")
        legacy_branch = self.mcp.index(
            "else if (effect == IOT_LIGHT_EFFECT_WARNING", name_branch
        )
        self.assertLess(name_branch, legacy_branch)
        self.assertIn("effect = light_effect_from_name(effect_name);", self.mcp)

    def test_p4_alternates_static_brightness_without_a_new_task(self):
        self.assertIn("mqtt_start_rgb_blink", self.mqtt)
        self.assertIn("mqtt_rgb_blink_tick", self.mqtt)
        self.assertIn("0, IOT_LIGHT_EFFECT_STATIC, 0", self.mqtt)
        self.assertIn(
            "s_rgb_blink.brightness, IOT_LIGHT_EFFECT_STATIC, 0", self.mqtt
        )
        self.assertNotIn("xTaskCreate", self.mqtt)

    def test_existing_smart_home_task_drives_250ms_blink_ticks(self):
        self.assertIn("mqtt_rgb_blink_tick();", self.tasks)
        self.assertIn("vTaskDelay(pdMS_TO_TICKS(250));", self.tasks)
        self.assertIn("(slow_tick + 1) % 4", self.tasks)

    def test_new_rgb_command_cancels_pending_blink(self):
        send_function = self.mqtt.index('extern "C" void mqtt_send_rgb_light')
        start_function = self.mqtt.index('extern "C" bool mqtt_start_rgb_blink')
        send_body = self.mqtt[send_function:start_function]
        self.assertIn("s_rgb_blink.active = false;", send_body)

    def test_warning_scene_does_not_force_red(self):
        self.assertIn(
            "case AMBIENT_SCENE_WARNING:     fill_rgb(index, s->red, s->green, s->blue);",
            self.ambient,
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
