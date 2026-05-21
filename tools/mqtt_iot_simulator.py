#!/usr/bin/env python3
"""
MQTT slave simulator for the Xiaozhi smart-home IoT link.

It simulates the three ESP32-S3 floor controllers:
  - subscribes to xiaozhi/iot/cmd/broadcast and xiaozhi/iot/cmd/{floor}
  - publishes announce, command ACK, and V2 heartbeat packets
  - can inject rain/fire/help sensor states for LVGL and Xiaozhi alarm tests

No third-party Python packages are required.
"""

from __future__ import annotations

import argparse
import signal
import socket
import struct
import sys
import time
from dataclasses import dataclass, field
from typing import Callable, Dict, Iterable, List, Optional, Tuple


MQTT_TOPIC_CMD_BROADCAST = "xiaozhi/iot/cmd/broadcast"
MQTT_TOPIC_CMD_PREFIX = "xiaozhi/iot/cmd/"
MQTT_TOPIC_RESP_PREFIX = "xiaozhi/iot/resp/"
MQTT_TOPIC_HEARTBEAT_PREFIX = "xiaozhi/iot/heartbeat/"
MQTT_TOPIC_ANNOUNCE_PREFIX = "xiaozhi/iot/announce/"

IOT_CMD_SET_GPIO = 0x01
IOT_CMD_GET_GPIO = 0x02
IOT_CMD_GET_ALL_GPIO = 0x03
IOT_CMD_HEARTBEAT = 0x04
IOT_CMD_DISCOVER = 0x06
IOT_CMD_ANNOUNCE_V2 = 0x08
IOT_CMD_SET_SERVO = 0x10
IOT_CMD_SET_LIGHT = 0x11
IOT_CMD_SET_RELAY = 0x12
IOT_CMD_BROADCAST_ALL_OFF = 0x30
IOT_CMD_BROADCAST_ALL_ON = 0x31
IOT_CMD_BROADCAST_LIGHTS_OFF = 0x32
IOT_CMD_BROADCAST_LIGHTS_ON = 0x33
IOT_CMD_EMERGENCY = 0x34

IOT_PROTOCOL_VERSION = 2
IOT_MAX_LIGHTS = 3
IOT_MAX_RELAYS = 3
IOT_MAX_SERVOS = 3

CMD_PACKET = struct.Struct("<BBBB4B")
ANNOUNCE_V2_PACKET = struct.Struct("<BBBB24s")
HEARTBEAT_V2_PACKET = struct.Struct("<BBBB24s13sBBBB3B3B3BHHBBBI8s")


def fixed_ascii(text: str, size: int) -> bytes:
    raw = text.encode("ascii", "replace")[:size]
    return raw + b"\0" * (size - len(raw))


def encode_remaining_length(value: int) -> bytes:
    out = bytearray()
    while True:
        encoded = value % 128
        value //= 128
        if value:
            encoded |= 0x80
        out.append(encoded)
        if not value:
            return bytes(out)


def mqtt_string(text: str) -> bytes:
    raw = text.encode("utf-8")
    return struct.pack("!H", len(raw)) + raw


class MiniMqttClient:
    def __init__(self, host: str, port: int, client_id: str, keepalive: int = 45):
        self.host = host
        self.port = port
        self.client_id = client_id
        self.keepalive = keepalive
        self.sock: Optional[socket.socket] = None
        self.packet_id = 1
        self.last_ping = 0.0
        self.on_publish: Optional[Callable[[str, bytes], None]] = None

    def connect(self) -> None:
        self.sock = socket.create_connection((self.host, self.port), timeout=10)
        self.sock.settimeout(1.0)

        variable_header = mqtt_string("MQTT") + bytes([4, 2]) + struct.pack("!H", self.keepalive)
        payload = mqtt_string(self.client_id)
        self._send_packet(0x10, variable_header + payload)

        packet_type, _flags, payload = self._recv_packet()
        if packet_type != 2 or len(payload) < 2 or payload[1] != 0:
            raise RuntimeError(f"MQTT CONNACK failed: type={packet_type} payload={payload.hex()}")
        self.last_ping = time.monotonic()

    def subscribe(self, topics: Iterable[str]) -> None:
        body = bytearray()
        packet_id = self._next_packet_id()
        body += struct.pack("!H", packet_id)
        for topic in topics:
            body += mqtt_string(topic)
            body.append(0)
        self._send_packet(0x82, bytes(body))

    def publish(self, topic: str, payload: bytes, retain: bool = False) -> None:
        fixed = 0x31 if retain else 0x30
        self._send_packet(fixed, mqtt_string(topic) + payload)

    def poll(self, timeout_s: float = 1.0) -> None:
        assert self.sock is not None
        self.sock.settimeout(timeout_s)
        try:
            packet_type, flags, payload = self._recv_packet()
        except socket.timeout:
            self._maybe_ping()
            return

        if packet_type == 3:
            self._handle_publish(flags, payload)
        elif packet_type == 13:
            return
        elif packet_type == 9:
            return
        self._maybe_ping()

    def close(self) -> None:
        if self.sock:
            try:
                self._send_packet(0xE0, b"")
            except OSError:
                pass
            self.sock.close()
            self.sock = None

    def _next_packet_id(self) -> int:
        packet_id = self.packet_id
        self.packet_id = 1 if self.packet_id >= 0xFFFF else self.packet_id + 1
        return packet_id

    def _send_packet(self, fixed_header: int, payload: bytes) -> None:
        assert self.sock is not None
        self.sock.sendall(bytes([fixed_header]) + encode_remaining_length(len(payload)) + payload)

    def _recv_packet(self) -> Tuple[int, int, bytes]:
        assert self.sock is not None
        first = self._read_exact(1)[0]
        multiplier = 1
        remaining = 0
        while True:
            encoded = self._read_exact(1)[0]
            remaining += (encoded & 0x7F) * multiplier
            if (encoded & 0x80) == 0:
                break
            multiplier *= 128
            if multiplier > 128 * 128 * 128:
                raise RuntimeError("Malformed MQTT remaining length")
        payload = self._read_exact(remaining) if remaining else b""
        return first >> 4, first & 0x0F, payload

    def _read_exact(self, size: int) -> bytes:
        assert self.sock is not None
        chunks = bytearray()
        while len(chunks) < size:
            data = self.sock.recv(size - len(chunks))
            if not data:
                raise ConnectionError("MQTT socket closed")
            chunks.extend(data)
        return bytes(chunks)

    def _handle_publish(self, flags: int, payload: bytes) -> None:
        if len(payload) < 2:
            return
        topic_len = struct.unpack("!H", payload[:2])[0]
        topic_start = 2
        topic_end = topic_start + topic_len
        if len(payload) < topic_end:
            return
        topic = payload[topic_start:topic_end].decode("utf-8", "replace")
        qos = (flags >> 1) & 0x03
        data_start = topic_end + (2 if qos else 0)
        data = payload[data_start:]
        if self.on_publish:
            self.on_publish(topic, data)

    def _maybe_ping(self) -> None:
        now = time.monotonic()
        if now - self.last_ping > max(5, self.keepalive // 2):
            self._send_packet(0xC0, b"")
            self.last_ping = now


@dataclass
class VirtualFloor:
    device_id: int
    name: str
    mac: str
    light_count: int
    relay_count: int
    servo_count: int
    sensor_count: int
    lights: List[int] = field(default_factory=lambda: [0, 0, 0])
    relays: List[int] = field(default_factory=lambda: [0, 0, 0])
    servos: List[int] = field(default_factory=lambda: [0, 0, 0])
    smoke_mv: int = 0
    rain_mv: int = 3300
    fire_status: int = 0
    rain_status: int = 0
    help_status: int = 0
    started_at: float = field(default_factory=time.monotonic)

    def pack_announce(self) -> bytes:
        return ANNOUNCE_V2_PACKET.pack(
            IOT_CMD_ANNOUNCE_V2,
            self.device_id,
            self.light_count + self.relay_count + self.servo_count,
            0,
            fixed_ascii(self.name, 24),
        )

    def pack_heartbeat(self) -> bytes:
        return HEARTBEAT_V2_PACKET.pack(
            IOT_CMD_HEARTBEAT,
            IOT_PROTOCOL_VERSION,
            self.device_id,
            1,
            fixed_ascii(self.name, 24),
            fixed_ascii(self.mac, 13),
            self.light_count,
            self.relay_count,
            self.servo_count,
            self.sensor_count,
            *self._fixed3(self.lights),
            *self._fixed3(self.relays),
            *self._fixed3(self.servos),
            self.smoke_mv,
            self.rain_mv,
            self.fire_status,
            self.rain_status,
            self.help_status,
            int(time.monotonic() - self.started_at),
            b"\0" * 8,
        )

    def pack_ack(self, command: int, gpio_index: int, value: int) -> bytes:
        return CMD_PACKET.pack(command, self.device_id, gpio_index & 0xFF, value & 0xFF, 0, 0, 0, 0)

    def apply_command(self, command: int, gpio_index: int, value: int) -> bool:
        changed = False
        if command == IOT_CMD_SET_GPIO:
            if gpio_index < self.light_count:
                self.lights[gpio_index] = 1 if value else 0
                changed = True
            elif gpio_index < self.light_count + self.relay_count:
                relay_index = gpio_index - self.light_count
                self.relays[relay_index] = 1 if value else 0
                changed = True
        elif command == IOT_CMD_SET_LIGHT and gpio_index < self.light_count:
            self.lights[gpio_index] = 1 if value else 0
            changed = True
        elif command == IOT_CMD_SET_RELAY and gpio_index < self.relay_count:
            self.relays[gpio_index] = 1 if value else 0
            changed = True
        elif command == IOT_CMD_SET_SERVO and 6 <= gpio_index < 6 + self.servo_count:
            self.servos[gpio_index - 6] = max(0, min(180, value))
            changed = True
        elif command == IOT_CMD_BROADCAST_ALL_OFF:
            self.lights = [0, 0, 0]
            self.relays = [0, 0, 0]
            self.servos = [0, 0, 0]
            changed = True
        elif command == IOT_CMD_BROADCAST_ALL_ON:
            self.lights = [1, 1, 1]
            self.relays = [1, 1, 1]
            self.servos = [180, 180, 180]
            changed = True
        elif command == IOT_CMD_BROADCAST_LIGHTS_OFF:
            self.lights = [0, 0, 0]
            changed = True
        elif command == IOT_CMD_BROADCAST_LIGHTS_ON:
            self.lights = [1, 1, 1]
            changed = True
        elif command == IOT_CMD_EMERGENCY:
            self.help_status = 1
            changed = True
        return changed

    def set_fire(self, active: bool) -> None:
        self.fire_status = 2 if active else 0
        self.smoke_mv = 2200 if active else 350

    def set_rain(self, active: bool) -> None:
        self.rain_status = 1 if active else 0
        self.rain_mv = 1200 if active else 3300

    @staticmethod
    def _fixed3(values: List[int]) -> List[int]:
        fixed = (values + [0, 0, 0])[:3]
        return [int(v) & 0xFF for v in fixed]


class SlaveSimulator:
    def __init__(self, mqtt: MiniMqttClient, floors: Dict[int, VirtualFloor], verbose: bool):
        self.mqtt = mqtt
        self.floors = floors
        self.verbose = verbose
        self.mqtt.on_publish = self.on_publish

    def publish_announce(self, floor: VirtualFloor) -> None:
        topic = MQTT_TOPIC_ANNOUNCE_PREFIX + floor.mac
        self.mqtt.publish(topic, floor.pack_announce(), retain=True)
        self.log(f"announce floor={floor.device_id} topic={topic}")

    def publish_heartbeat(self, floor: VirtualFloor) -> None:
        topic = MQTT_TOPIC_HEARTBEAT_PREFIX + floor.mac
        self.mqtt.publish(topic, floor.pack_heartbeat())
        self.log(
            f"heartbeat floor={floor.device_id} lights={floor.lights[:floor.light_count]} "
            f"relays={floor.relays[:floor.relay_count]} servos={floor.servos[:floor.servo_count]} "
            f"smoke={floor.smoke_mv} rain={floor.rain_mv} fire={floor.fire_status}"
        )

    def publish_ack(self, floor: VirtualFloor, command: int, gpio_index: int, value: int) -> None:
        topic = MQTT_TOPIC_RESP_PREFIX + floor.mac
        self.mqtt.publish(topic, floor.pack_ack(command, gpio_index, value))
        self.log(f"ack floor={floor.device_id} cmd=0x{command:02X} gpio={gpio_index} value={value}")

    def publish_all(self) -> None:
        for floor in self.floors.values():
            self.publish_announce(floor)
            self.publish_heartbeat(floor)

    def on_publish(self, topic: str, payload: bytes) -> None:
        if topic == MQTT_TOPIC_CMD_BROADCAST:
            targets = list(self.floors.values())
        elif topic.startswith(MQTT_TOPIC_CMD_PREFIX):
            try:
                floor_id = int(topic.removeprefix(MQTT_TOPIC_CMD_PREFIX))
            except ValueError:
                return
            target = self.floors.get(floor_id)
            targets = [target] if target else []
        else:
            return

        if len(payload) < CMD_PACKET.size:
            self.log(f"ignore short command topic={topic} len={len(payload)}")
            return

        command, _device_id, gpio_index, value, *_reserved = CMD_PACKET.unpack(payload[:CMD_PACKET.size])
        for floor in targets:
            if command == IOT_CMD_DISCOVER:
                self.publish_announce(floor)
                self.publish_heartbeat(floor)
                continue
            floor.apply_command(command, gpio_index, value)
            self.publish_ack(floor, command, gpio_index, value)
            self.publish_heartbeat(floor)

    def log(self, text: str) -> None:
        if self.verbose:
            print(f"[sim] {text}", flush=True)


def default_floors(ids: Iterable[int]) -> Dict[int, VirtualFloor]:
    templates = {
        1: VirtualFloor(1, "1F_Device", "A10000000001", light_count=1, relay_count=0, servo_count=1, sensor_count=0),
        2: VirtualFloor(2, "2F_Device", "A20000000002", light_count=3, relay_count=3, servo_count=3, sensor_count=2),
        3: VirtualFloor(3, "3F_Device", "A30000000003", light_count=3, relay_count=3, servo_count=3, sensor_count=5, smoke_mv=350),
    }
    return {floor_id: templates[floor_id] for floor_id in ids}


def parse_floor_ids(text: str) -> List[int]:
    floor_ids = []
    for part in text.split(","):
        if not part.strip():
            continue
        floor_id = int(part)
        if floor_id not in (1, 2, 3):
            raise argparse.ArgumentTypeError("floors must be a comma-separated subset of 1,2,3")
        floor_ids.append(floor_id)
    return sorted(set(floor_ids))


def run_self_test() -> None:
    assert CMD_PACKET.size == 8, CMD_PACKET.size
    assert ANNOUNCE_V2_PACKET.size == 28, ANNOUNCE_V2_PACKET.size
    assert HEARTBEAT_V2_PACKET.size == 73, HEARTBEAT_V2_PACKET.size

    floor = default_floors([3])[3]
    floor.set_fire(True)
    heartbeat = floor.pack_heartbeat()
    unpacked = HEARTBEAT_V2_PACKET.unpack(heartbeat)
    assert unpacked[0] == IOT_CMD_HEARTBEAT
    assert unpacked[1] == IOT_PROTOCOL_VERSION
    assert unpacked[2] == 3
    assert unpacked[19] == 2200
    assert unpacked[21] == 2

    changed = floor.apply_command(IOT_CMD_SET_SERVO, 7, 180)
    assert changed
    assert floor.servos[1] == 180
    ack = floor.pack_ack(IOT_CMD_SET_SERVO, 7, 180)
    assert CMD_PACKET.unpack(ack)[:4] == (IOT_CMD_SET_SERVO, 3, 7, 180)
    print("self-test passed")


def main() -> int:
    parser = argparse.ArgumentParser(description="Simulate Xiaozhi MQTT IoT floor controllers.")
    parser.add_argument("--broker", default="8.134.167.240", help="MQTT broker host")
    parser.add_argument("--port", type=int, default=1883, help="MQTT broker port")
    parser.add_argument("--client-id", default="xiaozhi_iot_simulator", help="MQTT client ID")
    parser.add_argument("--floors", type=parse_floor_ids, default=parse_floor_ids("1,2,3"), help="Floors to simulate, e.g. 1,2,3")
    parser.add_argument("--interval", type=float, default=5.0, help="Heartbeat interval in seconds")
    parser.add_argument("--fire-floor", type=int, choices=[1, 2, 3], help="Inject fire alarm on this floor")
    parser.add_argument("--fire-after", type=float, default=0.0, help="Seconds before fire injection")
    parser.add_argument("--clear-fire-after", type=float, help="Seconds after start to clear the injected fire")
    parser.add_argument("--rain-floor", type=int, choices=[1, 2, 3], help="Inject rain status on this floor")
    parser.add_argument("--once", action="store_true", help="Publish announce and heartbeat once, then exit")
    parser.add_argument("--self-test", action="store_true", help="Run protocol packing tests without network")
    parser.add_argument("--verbose", action="store_true", help="Print every simulated publish")
    args = parser.parse_args()

    if args.self_test:
        run_self_test()
        return 0

    floors = default_floors(args.floors)
    if args.rain_floor in floors:
        floors[args.rain_floor].set_rain(True)

    mqtt = MiniMqttClient(args.broker, args.port, args.client_id)
    sim = SlaveSimulator(mqtt, floors, args.verbose)
    stop = False

    def request_stop(_signum, _frame) -> None:
        nonlocal stop
        stop = True

    signal.signal(signal.SIGINT, request_stop)
    signal.signal(signal.SIGTERM, request_stop)

    mqtt.connect()
    topics = [MQTT_TOPIC_CMD_BROADCAST] + [MQTT_TOPIC_CMD_PREFIX + str(floor_id) for floor_id in floors]
    mqtt.subscribe(topics)
    print(f"connected to mqtt://{args.broker}:{args.port}; simulating floors {sorted(floors)}")

    sim.publish_all()
    if args.once:
        mqtt.close()
        return 0

    started = time.monotonic()
    next_heartbeat = 0.0
    fire_set = False
    fire_cleared = False

    while not stop:
        now = time.monotonic()
        elapsed = now - started

        if args.fire_floor in floors and not fire_set and elapsed >= args.fire_after:
            floors[args.fire_floor].set_fire(True)
            sim.publish_heartbeat(floors[args.fire_floor])
            print(f"fire injected on floor {args.fire_floor}")
            fire_set = True

        if (
            args.fire_floor in floors
            and args.clear_fire_after is not None
            and not fire_cleared
            and elapsed >= args.clear_fire_after
        ):
            floors[args.fire_floor].set_fire(False)
            sim.publish_heartbeat(floors[args.fire_floor])
            print(f"fire cleared on floor {args.fire_floor}")
            fire_cleared = True

        if now >= next_heartbeat:
            for floor in floors.values():
                sim.publish_heartbeat(floor)
            next_heartbeat = now + args.interval

        mqtt.poll(timeout_s=0.5)

    mqtt.close()
    print("simulator stopped")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        raise SystemExit(130)
