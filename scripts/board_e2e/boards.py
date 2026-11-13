"""Board-specific tooling and configuration validation, separate from scenarios."""

import ipaddress
import json
import os
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class Board:
    environment: str
    unit_environment: str
    chip: str
    flash_bytes: int
    header: str


BOARDS = {
    "esp32": Board("datastream-api-esp32", "test-esp32", "esp32", 4 * 1024 * 1024,
                   "platform_esp32.h"),
    "esp8266": Board("datastream-api-esp8266", "test-esp8266", "esp8266", 4 * 1024 * 1024,
                     "platform_esp8266.h"),
}


def load_config(path):
    value = json.loads(Path(path).read_text())
    if not isinstance(value, dict) or value.get("development") is not True:
        raise ValueError("configuration must explicitly set development: true")
    for key, limit in (("wifi_ssid", 32), ("wifi_password", 63), ("provision_token", 64)):
        text = value.get(key)
        if not isinstance(text, str) or not text or len(text.encode()) > limit or "\0" in text:
            raise ValueError(f"invalid {key}")
    # Only a private LAN interface and a loopback Engine are supported. This
    # runner must never accidentally bootstrap a board against production.
    try:
        address = ipaddress.ip_address(value.get("lan_address", ""))
    except (ValueError, TypeError):
        raise ValueError("invalid lan_address") from None
    private_lans = ("10.0.0.0/8", "172.16.0.0/12", "192.168.0.0/16")
    if address.version != 4 or not any(address in ipaddress.ip_network(n) for n in private_lans):
        raise ValueError("lan_address must be the laptop's private IPv4 LAN address")
    value["lan_address"] = str(address)
    for key, default in (("engine_port", 4000), ("tls_port", 9443)):
        port = value.setdefault(key, default)
        if type(port) is not int or not 1024 <= port <= 65535:
            raise ValueError(f"invalid {key}: use an unprivileged TCP port")
    if value["engine_port"] == value["tls_port"]:
        raise ValueError("Engine and TLS ingress ports must differ")
    return value


def redact(text, config):
    for key in ("wifi_ssid", "wifi_password", "provision_token"):
        secret = config.get(key, "")
        if secret:
            text = text.replace(secret, "[redacted]").replace(
                json.dumps(secret, ensure_ascii=True)[1:-1], "[redacted]")
    return text


def esptool_path():
    core = Path(os.environ.get("PLATFORMIO_CORE_DIR", Path.home() / ".platformio"))
    path = core / "packages/tool-esptoolpy/esptool.py"
    if not path.is_file():
        raise RuntimeError("PlatformIO esptool not found; build the selected board environment first")
    return path
