import os
import sys
import tempfile
import time
import unittest
from pathlib import Path
from unittest.mock import patch

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

try:
    from PyQt6.QtWidgets import QApplication, QMessageBox
    from RemoteSD import output_device
except ImportError:
    output_device = None


class FakeMessage:
    messages = []

    def __init__(self, text, **kwargs):
        self.text = text
        self.messages.append(self)

    def show(self):
        pass

    def hide(self):
        pass

    def setText(self, text):
        self.text = text

    def setProgress(self, progress):
        self.progress = progress


@unittest.skipIf(output_device is None, "Cura/Uranium and PyQt6 are required")
class OutputDeviceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = QApplication.instance() or QApplication([])

    def setUp(self):
        FakeMessage.messages = []
        self.messages = patch.object(output_device, "Message", FakeMessage)
        self.messages.start()
        self.addCleanup(self.messages.stop)
        self.output = output_device.RemoteSDOutputDevice()

    def wait_done(self):
        deadline = time.monotonic() + 5
        while self.output._busy and time.monotonic() < deadline:
            self.app.processEvents()
            time.sleep(0.005)
        self.assertFalse(self.output._busy)

    def test_scan_failure_is_visible(self):
        with patch.object(output_device.client, "discover", side_effect=RuntimeError("network down")):
            self.output.requestWrite([], "part")
            self.wait_done()
        self.assertIn("network down", FakeMessage.messages[-1].text)

    def test_plugin_registers_and_removes_save_option(self):
        manager = unittest.mock.Mock()
        application = unittest.mock.Mock()
        application.getOutputDeviceManager.return_value = manager
        with patch("UM.OutputDevice.OutputDevicePlugin.Application.getInstance", return_value=application):
            plugin = output_device.RemoteSDPlugin()
            plugin.start()
            registered = manager.addOutputDevice.call_args.args[0]
            self.assertEqual(registered.getShortDescription(), "Save to Remote-SD")
            self.assertEqual(registered.getDescription(), "Save to Remote-SD")
            plugin.stop()
        manager.removeOutputDevice.assert_called_once_with("remote_sd")

    def test_no_devices_is_visible(self):
        with patch.object(output_device.client, "discover", return_value=[]):
            self.output.requestWrite([], "part")
            self.wait_done()
        self.assertIn("No Remote SD devices found", FakeMessage.messages[-1].text)

    def test_confirmation_cancel_does_not_write(self):
        device = output_device.client.Device("id", "Printer", "127.0.0.1", 80)
        with patch.object(output_device.client, "discover", return_value=[device]), \
                patch.object(output_device.QMessageBox, "question", return_value=QMessageBox.StandardButton.No), \
                patch.object(output_device.client, "upload") as upload:
            self.output.requestWrite([], "part")
            self.wait_done()
            upload.assert_not_called()

    def test_choice_writer_upload_and_cleanup(self):
        devices = [output_device.client.Device("a", "One", "127.0.0.1", 80),
                   output_device.client.Device("b", "Two", "127.0.0.2", 80)]
        captured = {}

        class FakeSignal:
            def connect(self, callback):
                self.callback = callback

        class FakeJob:
            def __init__(self, writer, stream, nodes, mode):
                self.stream = stream
                self.finished = FakeSignal()
                captured["nodes"] = nodes

            def start(self):
                self.stream.write("G28\n")
                self.finished.callback(self)

            def getStream(self):
                return self.stream

            def getResult(self):
                return True

            def hasError(self):
                return False

        def fake_upload(device, path, filename, progress):
            captured.update(device=device, path=path, filename=filename, data=Path(path).read_text())
            progress(100)

        class Registry:
            def getPluginObject(self, name):
                self.name = name
                return object()

        registry = Registry()
        with patch.object(output_device.client, "discover", return_value=devices), \
                patch.object(output_device.QInputDialog, "getItem", return_value=("Two (b, 127.0.0.2:80)", True)), \
                patch.object(output_device.QMessageBox, "question", return_value=QMessageBox.StandardButton.Yes), \
                patch.object(output_device.PluginRegistry, "getInstance", return_value=registry), \
                patch.object(output_device, "WriteFileJob", FakeJob), \
                patch.object(output_device.client, "upload", side_effect=fake_upload):
            self.output.requestWrite(["scene"], "model")
            self.wait_done()
        self.assertEqual(registry.name, "GCodeWriter")
        self.assertEqual(captured["device"], devices[1])
        self.assertEqual(captured["data"], "G28\n")
        self.assertEqual(captured["nodes"], ["scene"])
        self.assertEqual(captured["filename"], "model.gcode")
        self.assertFalse(Path(captured["path"]).exists())
        self.assertIn("Upload complete", FakeMessage.messages[-1].text)

    def test_upload_error_is_visible_and_temp_file_removed(self):
        with tempfile.NamedTemporaryFile(delete=False) as file:
            path = file.name
        self.output._path = path
        self.output._busy = True
        self.output._on_uploaded("HTTP 500")
        self.assertFalse(Path(path).exists())
        self.assertIn("partial file", FakeMessage.messages[-1].text)

    def test_writer_failure_cleans_up_and_does_not_upload(self):
        device = output_device.client.Device("id", "Printer", "127.0.0.1", 80)
        paths = []

        class FakeSignal:
            def connect(self, callback):
                self.callback = callback

        class FailingJob:
            def __init__(self, writer, stream, nodes, mode):
                self.stream = stream
                paths.append(stream.name)
                self.finished = FakeSignal()

            def start(self):
                self.finished.callback(self)

            def getStream(self):
                return self.stream

            def getResult(self):
                return False

            def hasError(self):
                return True

            def getError(self):
                return RuntimeError("no sliced G-code")

        registry = unittest.mock.Mock()
        registry.getPluginObject.return_value = object()
        with patch.object(output_device.client, "discover", return_value=[device]), \
                patch.object(output_device.QMessageBox, "question", return_value=QMessageBox.StandardButton.Yes), \
                patch.object(output_device.PluginRegistry, "getInstance", return_value=registry), \
                patch.object(output_device, "WriteFileJob", FailingJob), \
                patch.object(output_device.client, "upload") as upload:
            self.output.requestWrite([], "part")
            self.wait_done()
            upload.assert_not_called()
        self.assertIn("no sliced G-code", FakeMessage.messages[-1].text)
        self.assertFalse(Path(paths[0]).exists())


if __name__ == "__main__":
    unittest.main()
