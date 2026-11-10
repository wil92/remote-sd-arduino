import http.server
import json
import sys
import tempfile
import threading
import types
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from RemoteSD import client


class Handler(http.server.BaseHTTPRequestHandler):
    status = 204
    info = {"type": "remote-sd", "id": "remote-sd-123", "name": "Workshop", "api_version": 1}
    uploads = []

    def log_message(self, *args):
        pass

    def do_GET(self):
        if self.path != "/api/device":
            self.send_error(404)
            return
        data = json.dumps(self.info).encode()
        self.send_response(200)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_POST(self):
        data = self.rfile.read(int(self.headers["Content-Length"]))
        self.uploads.append((self.path, self.headers, data))
        self.send_response(self.status)
        self.end_headers()


class ClientTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()
        cls.thread.join()

    def setUp(self):
        Handler.status = 204
        Handler.info = {"type": "remote-sd", "id": "remote-sd-123", "name": "Workshop", "api_version": 1}
        Handler.uploads = []
        self.device = client.Device("remote-sd-123", "Workshop", "127.0.0.1", self.server.server_port)

    def test_filename_is_safe_and_has_one_extension(self):
        self.assertEqual(client.safe_filename("../../bad\"\r\nname.GCODE"), "bad___name.gcode")
        self.assertEqual(client.safe_filename("..."), "untitled_print.gcode")
        self.assertEqual(client.safe_filename("example.gcode"), "example.gcode")
        self.assertEqual(len(client.safe_filename("a" * 200)), 106)

    def test_identity_validation_and_upload(self):
        self.assertEqual(client.device_at("127.0.0.1", self.server.server_port), self.device)
        data = b"G28\nG1 X12 Y34\n" * 4000
        progress = []
        with tempfile.NamedTemporaryFile() as f:
            f.write(data)
            f.flush()
            client.upload(self.device, f.name, "sample.gcode", progress.append)
        path, headers, body = Handler.uploads[0]
        boundary = headers["Content-Type"].split("boundary=")[1].encode()
        self.assertEqual(path, "/upload")
        self.assertEqual(len(body), int(headers["Content-Length"]))
        self.assertIn(b'name="file"; filename="sample.gcode"', body)
        self.assertIn(b"\r\n\r\n" + data + b"\r\n--" + boundary + b"--\r\n", body)
        self.assertEqual(progress[-1], 100)
        self.assertEqual(progress, sorted(progress))

    def test_rejects_wrong_identity_and_upload_http_failure(self):
        Handler.info["type"] = "other"
        with self.assertRaisesRegex(ValueError, "Unsupported"):
            client.device_at("127.0.0.1", self.server.server_port)
        Handler.status = 500
        with tempfile.NamedTemporaryFile() as f:
            f.write(b"G28\n")
            f.flush()
            with self.assertRaisesRegex(RuntimeError, "HTTP 500"):
                client.upload(self.device, f.name, "sample", lambda _: None)

    def test_discovery_resolves_removes_and_deduplicates(self):
        device = self.device

        class FakeZeroconf:
            closed = False

            def get_service_info(self, service, name, timeout):
                self.service = service
                return types.SimpleNamespace(port=device.port, parsed_addresses=lambda: ["127.0.0.1"])

            def close(self):
                self.closed = True

        class FakeBrowser:
            def __init__(self, zc, service, listener):
                self.zc = zc
                self.listener = listener
                listener.add_service(zc, service, "first")
                listener.add_service(zc, service, "second")
                listener.add_service(zc, service, "removed")
                listener.remove_service(zc, service, "removed")

            def cancel(self):
                self.cancelled = True

        zc = FakeZeroconf()
        module = types.SimpleNamespace(Zeroconf=lambda: zc, ServiceBrowser=FakeBrowser)
        with patch.dict(sys.modules, {"zeroconf": module}):
            self.assertEqual(client.discover(0), [device])
        self.assertEqual(zc.service, client.SERVICE)
        self.assertTrue(zc.closed)

    def test_discovery_rejects_unrelated_or_unreachable_devices(self):
        Handler.info["api_version"] = 2
        with self.assertRaises(ValueError):
            client.device_at("127.0.0.1", self.server.server_port)
        with self.assertRaises(ValueError):
            client.device_at("224.0.0.1", 80)


if __name__ == "__main__":
    unittest.main()
