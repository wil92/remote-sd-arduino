---
name: device-logs
description: Inspect logs directly from the ESP32 device over its PlatformIO serial monitor.
---

# Check device logs

Use this workflow when the user requests device logs or runtime investigation
requires serial output.

1. Inspect `platformio.ini` and the relevant firmware logging code to identify
   the serial settings and expected messages. This firmware uses 115200 baud.
2. Use `pio device list` to identify available serial ports. If multiple
   devices are present or the intended target is unclear, ask the user which
   port belongs to the device. Do not guess or upload firmware.
3. Open `pio device monitor -e esp32-c3-devkitm-1 --baud 115200` from the project root;
   add `--port <port>` when the target port was identified. Monitor only long
   enough to capture the relevant output, then exit with Ctrl+C. If the
   monitor cannot run interactively, use a bounded capture compatible with
   the available terminal and report any failure instead of leaving it open.
4. Some serial adapters or monitor connections can reset the device. Do not
   deliberately reset or interrupt an active device without user approval.
   If startup output is needed but unavailable, ask before requesting a reset.
5. Treat logs as potentially sensitive: the setup hotspot password can be
   printed on serial. Never repeat credentials in the response or commit logs;
   redact other sensitive values and report only relevant observations.

If the port is unavailable, busy, or permission is denied, report the error
and ask the user to resolve it. Do not kill unrelated processes or change
device permissions without authorization.
