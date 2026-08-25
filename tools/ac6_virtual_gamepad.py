#!/usr/bin/env python3
"""Persistent localhost controller for an emulated Xbox 360 gamepad."""

from __future__ import annotations

import argparse
import atexit
import json
import os
import socket
import sys
import time
from pathlib import Path
from typing import Any

import vgamepad as vg


DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 37610
DEFAULT_STATUS_PATH = Path(".cache/ac6-virtual-gamepad/status.json")

BUTTONS = {
    "A": vg.XUSB_BUTTON.XUSB_GAMEPAD_A,
    "B": vg.XUSB_BUTTON.XUSB_GAMEPAD_B,
    "X": vg.XUSB_BUTTON.XUSB_GAMEPAD_X,
    "Y": vg.XUSB_BUTTON.XUSB_GAMEPAD_Y,
    "BACK": vg.XUSB_BUTTON.XUSB_GAMEPAD_BACK,
    "START": vg.XUSB_BUTTON.XUSB_GAMEPAD_START,
    "GUIDE": vg.XUSB_BUTTON.XUSB_GAMEPAD_GUIDE,
    "DPAD_UP": vg.XUSB_BUTTON.XUSB_GAMEPAD_DPAD_UP,
    "DPAD_DOWN": vg.XUSB_BUTTON.XUSB_GAMEPAD_DPAD_DOWN,
    "DPAD_LEFT": vg.XUSB_BUTTON.XUSB_GAMEPAD_DPAD_LEFT,
    "DPAD_RIGHT": vg.XUSB_BUTTON.XUSB_GAMEPAD_DPAD_RIGHT,
    "LB": vg.XUSB_BUTTON.XUSB_GAMEPAD_LEFT_SHOULDER,
    "RB": vg.XUSB_BUTTON.XUSB_GAMEPAD_RIGHT_SHOULDER,
    "LS": vg.XUSB_BUTTON.XUSB_GAMEPAD_LEFT_THUMB,
    "RS": vg.XUSB_BUTTON.XUSB_GAMEPAD_RIGHT_THUMB,
}


def clamp(value: float, minimum: float, maximum: float) -> float:
    return max(minimum, min(maximum, value))


def normalize_buttons(value: Any) -> list[str]:
    if value is None:
        return []
    if isinstance(value, str):
        value = [value]
    if not isinstance(value, list):
        raise ValueError("buttons must be a string or list")

    names = [str(name).upper() for name in value]
    unknown = sorted(set(names) - BUTTONS.keys())
    if unknown:
        raise ValueError(f"unknown buttons: {', '.join(unknown)}")
    return names


class VirtualPad:
    def __init__(self) -> None:
        self.gamepad = vg.VX360Gamepad()
        self.reset()
        atexit.register(self.reset)

    def reset(self) -> None:
        self.gamepad.reset()
        self.gamepad.update()

    def apply_state(self, state: dict[str, Any]) -> None:
        self.gamepad.reset()
        for name in normalize_buttons(state.get("buttons")):
            self.gamepad.press_button(button=BUTTONS[name])

        self.gamepad.left_joystick_float(
            x_value_float=clamp(float(state.get("lx", 0.0)), -1.0, 1.0),
            y_value_float=clamp(float(state.get("ly", 0.0)), -1.0, 1.0),
        )
        self.gamepad.right_joystick_float(
            x_value_float=clamp(float(state.get("rx", 0.0)), -1.0, 1.0),
            y_value_float=clamp(float(state.get("ry", 0.0)), -1.0, 1.0),
        )
        self.gamepad.left_trigger_float(
            value_float=clamp(float(state.get("lt", 0.0)), 0.0, 1.0)
        )
        self.gamepad.right_trigger_float(
            value_float=clamp(float(state.get("rt", 0.0)), 0.0, 1.0)
        )
        self.gamepad.update()

    def run_sequence(self, steps: list[dict[str, Any]], reset_after: bool) -> None:
        if not isinstance(steps, list) or not steps:
            raise ValueError("sequence steps must be a non-empty list")
        for step in steps:
            if not isinstance(step, dict):
                raise ValueError("each sequence step must be an object")
            duration_ms = int(step.get("duration_ms", 100))
            if duration_ms < 0 or duration_ms > 60_000:
                raise ValueError("duration_ms must be between 0 and 60000")
            self.apply_state(step)
            time.sleep(duration_ms / 1000.0)
        if reset_after:
            self.reset()


def write_status(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(".tmp")
    temporary.write_text(json.dumps(payload, sort_keys=True), encoding="utf-8")
    temporary.replace(path)


def execute_command(pad: VirtualPad, command: dict[str, Any]) -> tuple[dict[str, Any], bool]:
    action = str(command.get("action", "status")).lower()
    if action == "status":
        return {"ok": True, "pid": os.getpid(), "ready": True}, False
    if action == "reset":
        pad.reset()
        return {"ok": True, "action": action}, False
    if action == "set":
        pad.apply_state(command)
        return {"ok": True, "action": action}, False
    if action == "tap":
        duration_ms = int(command.get("duration_ms", 100))
        if duration_ms < 1 or duration_ms > 60_000:
            raise ValueError("duration_ms must be between 1 and 60000")
        pad.run_sequence(
            [{"buttons": command.get("buttons"), "duration_ms": duration_ms}],
            reset_after=True,
        )
        return {"ok": True, "action": action}, False
    if action == "sequence":
        pad.run_sequence(
            command.get("steps"),
            reset_after=bool(command.get("reset_after", True)),
        )
        return {"ok": True, "action": action}, False
    if action == "stop":
        pad.reset()
        return {"ok": True, "action": action}, True
    raise ValueError(f"unknown action: {action}")


def serve(host: str, port: int, status_path: Path) -> int:
    pad = VirtualPad()
    should_stop = False
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind((host, port))
        server.listen(4)
        write_status(
            status_path,
            {"host": host, "pid": os.getpid(), "port": port, "ready": True},
        )
        print(json.dumps({"host": host, "pid": os.getpid(), "port": port, "ready": True}))
        sys.stdout.flush()

        while not should_stop:
            connection, _ = server.accept()
            with connection:
                stream = connection.makefile("rwb")
                line = stream.readline(1_048_577)
                try:
                    if not line or len(line) > 1_048_576:
                        raise ValueError("missing or oversized command")
                    command = json.loads(line.decode("utf-8"))
                    if not isinstance(command, dict):
                        raise ValueError("command must be a JSON object")
                    response, should_stop = execute_command(pad, command)
                except Exception as error:
                    response = {
                        "ok": False,
                        "error": f"{type(error).__name__}: {error}",
                    }
                stream.write((json.dumps(response) + "\n").encode("utf-8"))
                stream.flush()

    write_status(
        status_path,
        {"host": host, "pid": os.getpid(), "port": port, "ready": False},
    )
    return 0


def send(host: str, port: int, command: dict[str, Any]) -> int:
    with socket.create_connection((host, port), timeout=5.0) as connection:
        stream = connection.makefile("rwb")
        stream.write((json.dumps(command) + "\n").encode("utf-8"))
        stream.flush()
        line = stream.readline(1_048_577)
    if not line:
        raise RuntimeError("virtual gamepad server closed without a response")
    response = json.loads(line.decode("utf-8"))
    print(json.dumps(response, sort_keys=True))
    return 0 if response.get("ok") else 1


def parse_command(args: argparse.Namespace) -> dict[str, Any]:
    if args.json:
        command = json.loads(args.json)
        if not isinstance(command, dict):
            raise ValueError("--json must contain a JSON object")
        return command
    if args.action == "tap":
        if not args.buttons:
            raise ValueError("tap requires at least one --button")
        return {
            "action": "tap",
            "buttons": args.buttons,
            "duration_ms": args.duration_ms,
        }
    if args.action == "set":
        return {
            "action": "set",
            "buttons": args.buttons,
            "lx": args.lx,
            "ly": args.ly,
            "rx": args.rx,
            "ry": args.ry,
            "lt": args.lt,
            "rt": args.rt,
        }
    return {"action": args.action}


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default=DEFAULT_HOST)
    parser.add_argument("--port", default=DEFAULT_PORT, type=int)
    subparsers = parser.add_subparsers(dest="mode", required=True)

    serve_parser = subparsers.add_parser("serve")
    serve_parser.add_argument("--status-path", type=Path, default=DEFAULT_STATUS_PATH)

    send_parser = subparsers.add_parser("send")
    send_parser.add_argument(
        "action",
        choices=("status", "reset", "set", "tap", "stop"),
        nargs="?",
        default="status",
    )
    send_parser.add_argument("--button", dest="buttons", action="append")
    send_parser.add_argument("--duration-ms", type=int, default=100)
    send_parser.add_argument("--lx", type=float, default=0.0)
    send_parser.add_argument("--ly", type=float, default=0.0)
    send_parser.add_argument("--rx", type=float, default=0.0)
    send_parser.add_argument("--ry", type=float, default=0.0)
    send_parser.add_argument("--lt", type=float, default=0.0)
    send_parser.add_argument("--rt", type=float, default=0.0)
    send_parser.add_argument("--json")
    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    try:
        if args.mode == "serve":
            return serve(args.host, args.port, args.status_path)
        return send(args.host, args.port, parse_command(args))
    except Exception as error:
        print(f"{type(error).__name__}: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
