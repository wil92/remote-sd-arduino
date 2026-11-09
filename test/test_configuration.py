import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class ConfigurationHeadersTest(unittest.TestCase):
    def setUp(self):
        self.temp_dir = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp_dir.cleanup)
        self.root = Path(self.temp_dir.name)
        for directory in ("src", "include", "lib/network"):
            (self.root / directory).mkdir(parents=True, exist_ok=True)
        for name in (
            "src/main.h",
            "lib/network/access_point_config.h",
            "include/access_point_config.example.h",
        ):
            shutil.copyfile(ROOT / name, self.root / name)

    def compile(self, source):
        result = subprocess.run(
            ["g++", "-std=c++11", "-x", "c++", "-fsyntax-only", "-"],
            input=source,
            text=True,
            capture_output=True,
            cwd=self.root,
        )
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_usb_defaults_without_local_header(self):
        self.compile("""
            #include "src/main.h"
            static_assert(ARDUINO_USB_MODE == 1, "USB mode");
            static_assert(ARDUINO_USB_CDC_ON_BOOT == 1, "USB CDC");
        """)

    def test_usb_local_values_override_defaults(self):
        (self.root / "include/usb_config.h").write_text(
            "#define ARDUINO_USB_MODE 0\n"
            "#define ARDUINO_USB_CDC_ON_BOOT 0\n"
        )
        self.compile("""
            #include "src/main.h"
            static_assert(ARDUINO_USB_MODE == 0, "USB mode");
            static_assert(ARDUINO_USB_CDC_ON_BOOT == 0, "USB CDC");
        """)

    def test_usb_missing_local_value_uses_default(self):
        (self.root / "include/usb_config.h").write_text(
            "#define ARDUINO_USB_CDC_ON_BOOT 0\n"
        )
        self.compile("""
            #include "src/main.h"
            static_assert(ARDUINO_USB_MODE == 1, "USB mode");
            static_assert(ARDUINO_USB_CDC_ON_BOOT == 0, "USB CDC");
        """)

    def test_access_point_example_and_local_override(self):
        header = '#include "lib/network/access_point_config.h"\n'
        self.compile(header + """
            static_assert(AP_SSID[0] == 'R', "example SSID");
            static_assert(AP_PASSWORD[0] == 'r', "example password");
        """)
        (self.root / "include/access_point_config.h").write_text(
            '#define AP_SSID "local access point"\n'
            '#define AP_PASSWORD "local password"\n'
        )
        self.compile(header + """
            static_assert(AP_SSID[0] == 'l', "local SSID");
            static_assert(AP_PASSWORD[0] == 'l', "local password");
        """)


if __name__ == "__main__":
    unittest.main()
