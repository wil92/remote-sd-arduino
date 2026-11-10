---
name: upload-firmware
description: Build and upload ESP32 firmware to an explicitly approved device using PlatformIO.
---

# Upload firmware

Use this workflow when a firmware upload is requested or needed.

1. Inspect `platformio.ini` and any relevant changes. Identify the target
   `esp32-c3-devkitm-1` board and confirm the intended device and serial port if
   multiple devices are connected or the target is otherwise uncertain.
2. Before **each upload attempt**, ask the user for explicit approval to
   overwrite the firmware on that device. A request to upload, an earlier
   approval, or approval of a build is not a substitute for this confirmation.
   Wait for approval; if it is declined or unavailable, do not upload.
3. From the project root, run `pio run -e esp32-c3-devkitm-1 -t upload`. If a specific
   port was confirmed, add `--upload-port <port>`. Do not change the device
   configuration, erase flash, or perform additional uploads without approval.
4. Report whether the build and upload succeeded, including actionable errors.
   Do not claim that runtime behavior was verified merely because the upload
   succeeded. Check device logs only when relevant, using the
   [`device-logs`](../device-logs/SKILL.md) skill.

Device tests and other commands that upload firmware require the same
per-attempt approval, even when they are not invoked through this skill.
