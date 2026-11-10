"""Discovery and HTTP transport, independent of Cura's UI."""

import http.client
import ipaddress
import json
import os
import re
import secrets
import socket
import time
from dataclasses import dataclass

SERVICE = "_remote-sd._tcp.local."
CONNECT_TIMEOUT = 5
UPLOAD_TIMEOUT = 120


@dataclass(frozen=True)
class Device:
    id: str
    name: str
    address: str
    port: int


def safe_filename(name):
    name = str(name or "untitled_print").replace("\\", "/").rsplit("/", 1)[-1]
    name = re.sub(r"[^A-Za-z0-9._ -]", "_", name).strip(" .")
    if name.lower().endswith(".gcode"):
        name = name[:-6].strip(" .")
    return (name[:100] or "untitled_print") + ".gcode"


def device_at(address, port, timeout=CONNECT_TIMEOUT):
    # mDNS is unauthenticated; reject anything other than a usable IP address.
    ip = ipaddress.ip_address(address)
    if ip.is_unspecified or ip.is_multicast:
        raise ValueError("Invalid device address")
    connection = http.client.HTTPConnection(str(ip), port, timeout=timeout)
    try:
        connection.request("GET", "/api/device", headers={"Connection": "close"})
        response = connection.getresponse()
        if response.status != 200:
            raise ValueError("Device information returned HTTP {}".format(response.status))
        payload = response.read(16385)
        if len(payload) > 16384:
            raise ValueError("Device information is too large")
        data = json.loads(payload)
        if (not isinstance(data, dict) or data.get("type") != "remote-sd"
                or data.get("api_version") != 1
                or not isinstance(data.get("id"), str) or not data["id"]
                or not isinstance(data.get("name"), str) or not data["name"]):
            raise ValueError("Unsupported device information")
        return Device(data["id"], data["name"], str(ip), port)
    finally:
        connection.close()


def discover(duration=3):
    # Imported here so the output option remains available if a dependency is missing.
    from zeroconf import Zeroconf, ServiceBrowser

    class Listener:
        def __init__(self):
            self.names = set()
            import threading
            self.lock = threading.Lock()

        def add_service(self, zc, service_type, name):
            with self.lock:
                self.names.add(name)

        def update_service(self, zc, service_type, name):
            self.add_service(zc, service_type, name)

        def remove_service(self, zc, service_type, name):
            with self.lock:
                self.names.discard(name)

    listener = Listener()
    zc = Zeroconf()
    browser = None
    try:
        browser = ServiceBrowser(zc, SERVICE, listener)
        time.sleep(duration)
        with listener.lock:
            names = sorted(listener.names)
        devices = {}
        for name in names:
            info = zc.get_service_info(SERVICE, name, timeout=1500)
            if not info or not 1 <= info.port <= 65535:
                continue
            for address in info.parsed_addresses():
                try:
                    device = device_at(address, info.port)
                except (OSError, ValueError, http.client.HTTPException):
                    continue
                devices[device.id] = device
                break
        return sorted(devices.values(), key=lambda device: (device.name, device.id))
    finally:
        if browser:
            browser.cancel()
        zc.close()


def upload(device, path, filename, progress=lambda percent: None):
    filename = safe_filename(filename)
    boundary = "RemoteSD" + secrets.token_hex(16)
    prefix = (
        "--{}\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"{}\"\r\n"
        "Content-Type: application/octet-stream\r\n\r\n"
    ).format(boundary, filename).encode("ascii")
    suffix = ("\r\n--{}--\r\n".format(boundary)).encode("ascii")
    size = os.path.getsize(path)
    connection = http.client.HTTPConnection(device.address, device.port, timeout=UPLOAD_TIMEOUT)
    try:
        connection.putrequest("POST", "/upload")
        connection.putheader("Content-Type", "multipart/form-data; boundary=" + boundary)
        connection.putheader("Content-Length", str(len(prefix) + size + len(suffix)))
        connection.putheader("Connection", "close")
        connection.endheaders()
        connection.send(prefix)
        sent = 0
        with open(path, "rb") as stream:
            while True:
                chunk = stream.read(16384)
                if not chunk:
                    break
                connection.send(chunk)
                sent += len(chunk)
                progress(int(sent * 100 / size))
        connection.send(suffix)
        response = connection.getresponse()
        response.read(4096)
        if not 200 <= response.status < 300:
            raise RuntimeError("Upload failed: HTTP {}".format(response.status))
    except socket.timeout as error:
        raise RuntimeError("Upload timed out; the device may have a partial file") from error
    finally:
        connection.close()
