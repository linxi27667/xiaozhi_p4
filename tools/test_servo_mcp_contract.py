#!/usr/bin/env python3
"""Regression checks for the AI-visible smart-home servo contract."""

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]
MCP_SOURCE = ROOT / "main/smart_home/mcp/smart_home_mcp_tool.cc"
MQTT_HEADER = ROOT / "main/smart_home/services/xiaozhi_mqtt.h"
MQTT_SOURCE = ROOT / "main/smart_home/services/xiaozhi_mqtt.cc"


class ServoMcpContractTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.mcp = MCP_SOURCE.read_text(encoding="utf-8")
        cls.mqtt_header = MQTT_HEADER.read_text(encoding="utf-8")
        cls.mqtt = MQTT_SOURCE.read_text(encoding="utf-8")

    def registration_kind(self, tool_name):
        match = re.search(
            rf'server\.(AddTool|AddUserOnlyTool)\("{re.escape(tool_name)}"',
            self.mcp,
        )
        self.assertIsNotNone(match, f"missing MCP tool registration: {tool_name}")
        return match.group(1)

    def test_only_semantic_servo_writer_is_ai_visible(self):
        self.assertEqual(
            self.registration_kind("self.iot.set_servo_power"), "AddTool"
        )
        for tool_name in (
            "self.iot.set_gpio",
            "self.iot.list_servos",
            "self.iot.set_servo",
            "self.iot.set_servo_by_index",
        ):
            with self.subTest(tool_name=tool_name):
                self.assertEqual(self.registration_kind(tool_name), "AddUserOnlyTool")

    def test_semantic_tool_description_routes_gate_directly(self):
        description_match = re.search(
            r'server\.AddTool\("self\.iot\.set_servo_power",\s*"([^"]+)"',
            self.mcp,
        )
        self.assertIsNotNone(description_match)
        description = description_match.group(1)
        self.assertIn("floor1_gate", description)
        self.assertIn("open=true", description)
        self.assertIn("do not call", description.lower())

    def test_mqtt_command_reports_publish_acceptance(self):
        self.assertRegex(
            self.mqtt_header,
            r"bool\s+mqtt_send_command\s*\(",
        )
        self.assertRegex(
            self.mqtt,
            r'if\s*\(!s_mqtt->Publish\(topic, payload, 0\)\)',
        )
        self.assertIn("return false;", self.mqtt)
        self.assertIn("return true;", self.mqtt)

    def test_home_scene_uses_canonical_gate_open_angle(self):
        home_case = re.search(
            r"case IOT_SCENE_HOME:(.*?)(?:break;)", self.mqtt, re.DOTALL
        )
        self.assertIsNotNone(home_case)
        self.assertIn(
            "mqtt_send_command(1, IOT_CMD_SET_SERVO, 6, 180);",
            home_case.group(1),
        )
        self.assertNotIn("IOT_CMD_SET_SERVO, 6, 135", home_case.group(1))

    def test_tool_and_servo_dispatch_are_observable(self):
        mcp_server = (ROOT / "main/mcp_server.cc").read_text(encoding="utf-8")
        self.assertIn('"tools/call: %s"', mcp_server)
        self.assertIn(
            '"Servo command: device_id=%s open=%s floor=%u index=%u angle=%u"',
            self.mcp,
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
