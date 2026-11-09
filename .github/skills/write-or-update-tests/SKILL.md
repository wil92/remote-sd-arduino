---
name: write-or-update-tests
description: Write or update tests for PlatformIO firmware changes, including feature behavior, edge cases, and regressions.
---

# Write or update tests

Use this skill when a change requires new or updated tests, including during
feature development.

1. Inspect the affected behavior, `platformio.ini`, and existing tests and test
   conventions. Identify expected outcomes, relevant edge cases, and regressions.
2. Add or update focused, deterministic tests using the existing framework and
   patterns where available. Exercise observable behavior rather than
   implementation details, and avoid real credentials or external services.
3. Use host-side tests where practical for logic that does not require hardware.
   For hardware-dependent behavior, document any required board, wiring, and
   setup without assuming unavailable hardware or changing configuration
   unnecessarily.
4. Run the relevant tests when possible and fix failures caused by the change.
   PlatformIO device tests may upload firmware: run them only with a suitable
   connected board, and ask for explicit approval before each upload attempt.
   Report tests that could not be run and any unresolved failures; do not
   claim unrun tests pass.
