import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class NetworkPollingTest(unittest.TestCase):
    def test_connection_and_hotspot_retries_poll_during_wait(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name in ("network_manager.cpp", "network_manager.h"):
                shutil.copyfile(ROOT / "lib/network" / name, root / name)

            (root / "WiFi.h").write_text("""
                #pragma once
                class IPAddress {
                public:
                    IPAddress() {}
                    IPAddress(int, int, int, int) {}
                };
                struct FakeWiFi {
                    int attempts = 0;
                    void softAPConfig(IPAddress, IPAddress, IPAddress) {}
                    bool softAP(const char *, const char *) {
                        return ++attempts > 1;
                    }
                };
                extern FakeWiFi WiFi;
                extern int delayCalls;
                inline void delay(int ms) {
                    if (ms == 100) ++delayCalls;
                }
                #define WL_CONNECTED 1
            """)
            (root / "WiFiMulti.h").write_text("""
                #pragma once
                class WiFiMulti {
                    int attempts = 0;
                public:
                    void addAP(const char *, const char *) {}
                    int run() { return ++attempts > 1 ? WL_CONNECTED : 0; }
                };
            """)
            (root / "main.cpp").write_text("""
                #include "network_manager.h"
                #include <cassert>
                FakeWiFi WiFi;
                int delayCalls = 0;
                int main() {
                    NetworkManager manager;
                    int polls = 0;
                    manager.connectToNetwork("ssid", "password",
                                             [&]() { ++polls; });
                    assert(polls == 10 && delayCalls == 10);
                    assert(manager.createHotspot("ssid", "password",
                                                 [&]() { ++polls; }));
                    assert(polls == 20 && delayCalls == 20);
                }
            """)
            subprocess.run(
                ["g++", "-std=c++11", "-I", str(root),
                 str(root / "network_manager.cpp"), str(root / "main.cpp"),
                 "-o", str(root / "test_network")],
                check=True,
                capture_output=True,
                text=True,
            )
            subprocess.run([str(root / "test_network")], check=True)


if __name__ == "__main__":
    unittest.main()
