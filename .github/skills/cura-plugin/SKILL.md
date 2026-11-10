---
name: cura-plugin
description: Run, build, and test the Remote SD Ultimaker Cura plugin.
---

# Cura plugin

Use this workflow for changes to `ultimate-cura-plugin`. Read
`../../../docs/cura-plugin.md`, the plugin code, and the relevant tests before editing.

1. Implement changes in `../../../ultimate-cura-plugin/RemoteSD`, following Cura SDK 8
   output-device APIs and the firmware discovery and upload contracts in
   `docs/discovery.md` and `docs/file-management.md`.
2. Run `python3 -m unittest discover -s ultimate-cura-plugin/tests -v`. Cura
   integration tests require Cura/Uranium and PyQt6 and otherwise skip; report
   skips. Add tests for changed behavior.
3. Build with `python3 ultimate-cura-plugin/build.py`. This downloads the pinned
   Python dependencies; build on the target OS and with a Python version
   compatible with the target Cura runtime. The resulting manual-install zip is
   `../../../ultimate-cura-plugin/dist/RemoteSD.zip`.
4. To run it in Cura, follow the manual installation instructions in
   `../../../docs/cura-plugin.md`. Do not modify the user's Cura configuration without
   permission; use an isolated configuration for smoke tests when possible.
5. Report whether a live Cura/device test was possible. Never upload firmware
   or run a device test that uploads firmware without separate explicit
   approval for each attempt.
