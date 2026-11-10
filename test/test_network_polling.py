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
            network = root / "network"
            network.mkdir()
            for name in ("network_manager.cpp", "network_manager.h"):
                shutil.copyfile(ROOT / "lib/network" / name, network / name)
            shutil.copyfile(ROOT / "lib/scan_status.h", root / "scan_status.h")

            (root / "WiFi.h").write_text("""
                #pragma once
                #include <string>
                #include <vector>
                using String = std::string;
                enum wl_status_t {
                    WL_IDLE_STATUS = 0,
                    WL_CONNECTED = 3,
                    WL_CONNECT_FAILED = 4,
                    WL_DISCONNECTED = 6
                };
                #define WIFI_STA 1
                #define WIFI_AP_STA 3
                #define WIFI_SCAN_RUNNING -1
                #define WIFI_SCAN_FAILED -2
                struct FakeSerial {
                    std::vector<String> lines;
                    String pending;
                    void print(const char *text) { pending += text; }
                    void println(int value) {
                        lines.push_back(pending + std::to_string(value));
                        pending.clear();
                    }
                    void println(const String &line) { lines.push_back(line); }
                };
                extern FakeSerial Serial;
                class IPAddress {
                public:
                    IPAddress() {}
                    IPAddress(int, int, int, int) {}
                    String toString() const { return "192.0.2.42"; }
                };
                struct FakeWiFi {
                    int attempts = 0;
                    int localIpCalls = 0;
                    bool persistentValue = true;
                    bool modeResult = true;
                    bool reconnectResult = true;
                    bool configResult = true;
                    int modeCalls = 0;
                    int modeValue = 0;
                    int beginCalls = 0;
                    int statusCalls = 0;
                    wl_status_t beginResult = WL_IDLE_STATUS;
                    std::vector<wl_status_t> statuses = {
                        WL_DISCONNECTED, WL_DISCONNECTED, WL_CONNECTED
                    };
                    std::vector<String> calls;
                    String ssid;
                    String password;
                    int scanResult = -1;
                    int scanState = -2;
                    int scanDeleteCalls = 0;
                    void persistent(bool enabled) {
                        calls.push_back("persistent");
                        persistentValue = enabled;
                    }
                    bool mode(int mode) {
                        calls.push_back("mode");
                        ++modeCalls;
                        modeValue = mode;
                        return modeResult;
                    }
                    bool setAutoReconnect(bool enabled) {
                        calls.push_back("auto-reconnect");
                        return enabled && reconnectResult;
                    }
                    wl_status_t begin(const char *network, const char *pass) {
                        calls.push_back("begin");
                        ++beginCalls;
                        ssid = network;
                        password = pass;
                        return beginResult;
                    }
                    wl_status_t status() {
                        calls.push_back("status");
                        size_t index = static_cast<size_t>(statusCalls++);
                        return statuses[index < statuses.size() ? index : statuses.size() - 1];
                    }
                    bool softAPConfig(IPAddress, IPAddress, IPAddress) {
                        return configResult;
                    }
                    bool softAP(const char *, const char *) {
                        return ++attempts > 1;
                    }
                    void scanDelete() { ++scanDeleteCalls; }
                    int scanNetworks(bool async) { return async ? scanResult : -2; }
                    int scanComplete() { return scanState; }
                    String SSID(int index) { return index == 0 ? "network" : ""; }
                    IPAddress localIP() { ++localIpCalls; return IPAddress(); }
                };
                extern FakeWiFi WiFi;
                extern int delayCalls;
                inline void delay(int ms) {
                    if (ms == 100) ++delayCalls;
                }
            """)
            (root / "ESPmDNS.h").write_text("""
                #pragma once
                #include "WiFi.h"
                struct FakeMDNS {
                    bool beginResult = true;
                    bool serviceResult = true;
                    int endCalls = 0;
                    String hostname;
                    String service;
                    String protocol;
                    int port = 0;
                    bool begin(const String &name) {
                        hostname = name;
                        return beginResult;
                    }
                    bool addService(const char *name, const char *proto, int p) {
                        service = name;
                        protocol = proto;
                        port = p;
                        return serviceResult;
                    }
                    void end() { ++endCalls; }
                };
                extern FakeMDNS MDNS;
            """)
            (root / "main.cpp").write_text("""
                #include "network/network_manager.h"
                #include <ESPmDNS.h>
                #include <cassert>
                FakeWiFi WiFi;
                FakeSerial Serial;
                FakeMDNS MDNS;
                int delayCalls = 0;
                int main() {
                    NetworkManager manager;
                    int polls = 0;
                    assert(manager.connectToNetwork("ssid", "password",
                                                    [&]() {
                                                        ++polls;
                                                        assert(WiFi.localIpCalls == 0);
                                                    }));
                    assert(polls == 20 && delayCalls == 20);
                    assert(!WiFi.persistentValue && WiFi.modeValue == WIFI_STA);
                    assert(WiFi.beginCalls == 1 && WiFi.statusCalls == 3);
                    assert(WiFi.ssid == "ssid" && WiFi.password == "password");
                    assert(WiFi.calls == std::vector<String>({
                        "persistent", "mode", "auto-reconnect", "begin",
                        "status", "status", "status"
                    }));
                    assert(WiFi.localIpCalls == 1);
                    assert(Serial.lines.size() == 2);
                    assert(Serial.lines[0] == "Wi-Fi connection status: 6");
                    assert(Serial.lines[1] == "Connected. IP address: 192.0.2.42");

                    WiFi = FakeWiFi();
                    Serial.lines.clear();
                    assert(!manager.connectToNetwork("", "password"));
                    assert(WiFi.calls.empty() && WiFi.beginCalls == 0);
                    assert(Serial.lines.back() == "Cannot connect: Wi-Fi configuration is invalid");
                    assert(!manager.connectToNetwork(nullptr, "password"));
                    assert(!manager.connectToNetwork("ssid", nullptr));

                    WiFi = FakeWiFi();
                    WiFi.modeResult = false;
                    assert(!manager.connectToNetwork("ssid", "password"));
                    assert(WiFi.beginCalls == 0 && WiFi.statusCalls == 0);
                    assert(Serial.lines.back() == "Cannot connect: failed to enable Wi-Fi station mode");

                    WiFi = FakeWiFi();
                    WiFi.reconnectResult = false;
                    assert(!manager.connectToNetwork("ssid", "password"));
                    assert(WiFi.beginCalls == 0 && WiFi.statusCalls == 0);
                    assert(Serial.lines.back() == "Cannot connect: failed to enable Wi-Fi auto-reconnect");

                    WiFi = FakeWiFi();
                    WiFi.beginResult = WL_CONNECT_FAILED;
                    assert(!manager.connectToNetwork("ssid", "password"));
                    assert(WiFi.beginCalls == 1 && WiFi.statusCalls == 0);
                    assert(Serial.lines.back() == "Cannot start Wi-Fi connection");

                    WiFi = FakeWiFi();
                    Serial.lines.clear();
                    WiFi.statuses = {WL_CONNECTED};
                    assert(manager.connectToNetwork("ssid", ""));
                    assert(WiFi.password.empty() && WiFi.localIpCalls == 1);
                    assert(Serial.lines.size() == 1);
                    assert(Serial.lines[0] == "Connected. IP address: 192.0.2.42");

                    WiFi = FakeWiFi();
                    assert(manager.createHotspot("ssid", "password",
                                                 [&]() { ++polls; }));
                    assert(polls == 30 && delayCalls == 30);
                    assert(WiFi.modeCalls == 1 && WiFi.modeValue == WIFI_AP_STA);
                    WiFi.modeResult = false;
                    assert(!manager.createHotspot("ssid", "password"));
                    WiFi.modeResult = true;
                    WiFi.configResult = false;
                    assert(!manager.createHotspot("ssid", "password"));
                    assert(manager.startScan() && WiFi.scanDeleteCalls == 1);
                    WiFi.scanState = -1;
                    assert(manager.scanComplete() == NETWORK_SCAN_RUNNING);
                    WiFi.scanState = 1;
                    assert(manager.scanComplete() == 1);
                    assert(manager.scanSsid(0) == "network");
                    WiFi.scanResult = -2;
                    assert(!manager.startScan() && WiFi.scanDeleteCalls == 2);
                    assert(manager.startDiscovery("remote-sd-001122334455"));
                    assert(MDNS.hostname == "remote-sd-001122334455");
                    assert(MDNS.service == "remote-sd");
                    assert(MDNS.protocol == "tcp" && MDNS.port == 80);
                    MDNS.beginResult = false;
                    MDNS.service = "";
                    assert(!manager.startDiscovery("test"));
                    assert(MDNS.service.empty() && MDNS.endCalls == 0);
                    MDNS.beginResult = true;
                    MDNS.serviceResult = false;
                    assert(!manager.startDiscovery("test"));
                    assert(MDNS.endCalls == 1);
                }
            """)
            subprocess.run(
                ["g++", "-std=c++11", "-I", str(root),
                 str(network / "network_manager.cpp"), str(root / "main.cpp"),
                 "-o", str(root / "test_network")],
                check=True,
                capture_output=True,
                text=True,
            )
            subprocess.run([str(root / "test_network")], check=True)


if __name__ == "__main__":
    unittest.main()
