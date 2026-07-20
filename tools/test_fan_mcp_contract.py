#!/usr/bin/env python3
"""Regression checks for the AI-visible fan control contract."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
MCP_SOURCE = ROOT / "main/smart_home/mcp/smart_home_mcp_tool.cc"
MODEL_SOURCE = ROOT / "main/smart_home/ui/model/mqtt_device_model.c"


class FanMcpContractTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.mcp = MCP_SOURCE.read_text(encoding="utf-8")
        cls.model = MODEL_SOURCE.read_text(encoding="utf-8")

    def test_fan_is_documented_as_second_floor_relay_zero(self):
        self.assertIn("fan/风扇 always call self.iot.set_relay", self.mcp)
        self.assertIn("fan/风扇 is the 2F relay at floor=2", self.mcp)

    def test_legacy_light_index_three_is_corrected(self):
        self.assertIn("if (floor == 2 && index == 3)", self.mcp)
        self.assertIn("command = IOT_CMD_SET_RELAY;", self.mcp)
        self.assertIn("index = 0;", self.mcp)

    def test_device_model_uses_relay_zero_for_fan(self):
        fan_mapping = next(
            line for line in self.model.splitlines() if '"floor2_fan"' in line
        )
        self.assertIn("IOT_CMD_SET_RELAY, 0", fan_mapping)


if __name__ == "__main__":
    unittest.main(verbosity=2)
