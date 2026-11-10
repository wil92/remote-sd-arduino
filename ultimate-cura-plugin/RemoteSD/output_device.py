import os
import sys
import tempfile
import threading

from PyQt6.QtCore import QObject, pyqtSignal
from PyQt6.QtWidgets import QInputDialog, QMessageBox
from UM.FileHandler.WriteFileJob import WriteFileJob
from UM.Mesh.MeshWriter import MeshWriter
from UM.Message import Message
from UM.OutputDevice.OutputDevice import OutputDevice
from UM.OutputDevice.OutputDevicePlugin import OutputDevicePlugin
from UM.PluginRegistry import PluginRegistry

from . import client

vendor = os.path.join(os.path.dirname(__file__), "vendor")
if os.path.isdir(vendor) and vendor not in sys.path:
    sys.path.insert(0, vendor)


class Bridge(QObject):
    discovered = pyqtSignal(object, object)
    uploaded = pyqtSignal(object)
    progress = pyqtSignal(int)


class RemoteSDPlugin(OutputDevicePlugin):
    def start(self):
        self._device = RemoteSDOutputDevice()
        self.getOutputDeviceManager().addOutputDevice(self._device)

    def stop(self):
        self.getOutputDeviceManager().removeOutputDevice("remote_sd")


class RemoteSDOutputDevice(OutputDevice):
    def __init__(self):
        super().__init__("remote_sd")
        self.setName("Remote SD")
        self.setShortDescription("Save to Remote-SD")
        self.setDescription("Save to Remote-SD")
        self.setIconName("save")
        self._bridge = Bridge()
        self._bridge.discovered.connect(self._on_discovered)
        self._bridge.uploaded.connect(self._on_uploaded)
        self._bridge.progress.connect(self._on_progress)
        self._busy = False
        self._path = None
        self._message = None

    def requestWrite(self, nodes, file_name=None, limit_mimetypes=False,
                     file_handler=None, filter_by_machine=False, **kwargs):
        if self._busy:
            Message("A Remote SD upload is already in progress.", title="Remote SD").show()
            return
        self._busy = True
        self._nodes = nodes
        self._filename = client.safe_filename(file_name)
        self._message = Message("Scanning the local network for Remote SD devices...",
                                title="Remote SD", progress=-1, lifetime=0,
                                dismissable=False, use_inactivity_timer=False)
        self._message.show()
        threading.Thread(target=self._scan, daemon=True).start()

    def _scan(self):
        try:
            devices = client.discover()
        except Exception as error:
            self._bridge.discovered.emit(None, str(error))
        else:
            self._bridge.discovered.emit(devices, None)

    def _on_discovered(self, devices, error):
        if error:
            self._finish("Discovery failed: " + error, failed=True)
            return
        if not devices:
            self._finish("No Remote SD devices found. Check the network and mDNS.", failed=True)
            return
        if len(devices) > 1:
            labels = ["{} ({}, {}:{})".format(d.name, d.id, d.address, d.port) for d in devices]
            selection, accepted = QInputDialog.getItem(None, "Remote SD", "Select a device:",
                                                       labels, 0, False)
            if not accepted:
                self._finish()
                return
            device = devices[labels.index(selection)]
        else:
            device = devices[0]
        confirmation = QMessageBox.question(
            None, "Save to Remote-SD",
            "Upload {} to {} ({})?\n\n"
            "An existing file with the same name will be replaced. "
            "Accessing the SD card can interrupt a print; do not upload while printing."
            .format(self._filename, device.name, device.address),
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
            QMessageBox.StandardButton.No,
        )
        if confirmation != QMessageBox.StandardButton.Yes:
            self._finish()
            return
        self._device = device
        self._message.setText("Preparing G-code...")
        self.writeStarted.emit(self)
        try:
            writer = PluginRegistry.getInstance().getPluginObject("GCodeWriter")
            if not writer:
                raise RuntimeError("Cura's GCodeWriter is unavailable.")
            stream = tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", newline="\n",
                                                  suffix=".gcode", delete=False)
            self._path = stream.name
            self._job = WriteFileJob(writer, stream, self._nodes, MeshWriter.OutputMode.TextMode)
            self._job.finished.connect(self._on_written)
            self._job.start()
        except Exception as exc:
            if "stream" in locals():
                stream.close()
            self._finish("Could not prepare G-code: " + str(exc), failed=True, started=True)

    def _on_written(self, job):
        try:
            job.getStream().close()
            if job.hasError() or not job.getResult():
                raise RuntimeError(str(job.getError() or "G-code writer failed"))
            self._message.setText("Uploading G-code...")
            self._message.setProgress(0)
            threading.Thread(target=self._upload, daemon=True).start()
        except Exception as error:
            self._finish("Could not prepare G-code: " + str(error), failed=True, started=True)

    def _upload(self):
        try:
            client.upload(self._device, self._path, self._filename, self._bridge.progress.emit)
        except Exception as error:
            self._bridge.uploaded.emit(str(error))
        else:
            self._bridge.uploaded.emit(None)

    def _on_progress(self, progress):
        self._message.setProgress(progress)
        if progress == 100:
            self._message.setText("Upload sent; waiting for the device to finish...")
        self.writeProgress.emit(self, progress)

    def _on_uploaded(self, error):
        if error:
            self._finish("Upload failed: {}. A partial file may remain.".format(error),
                         failed=True, started=True)
        else:
            self._finish("Upload complete: {}".format(self._filename), started=True)

    def _finish(self, text=None, failed=False, started=False):
        if self._message:
            self._message.hide()
            self._message = None
        if self._path:
            try:
                os.unlink(self._path)
            except OSError as error:
                Message("Could not remove temporary G-code: " + str(error), title="Remote SD").show()
            self._path = None
        self._busy = False
        if text:
            Message(text, title="Remote SD").show()
        if started:
            if failed:
                self.writeError.emit(self)
            else:
                self.writeSuccess.emit(self)
            self.writeFinished.emit(self)
