---
name: develop-feature
description: Develop a new feature for the PlatformIO ESP32 firmware, including implementation, tests, build, and validation.
---

# Develop a feature

Use this workflow when a user requests a new feature or an enhancement to the
firmware. Follow the user's instructions and any applicable repository
instructions throughout.

1. **Understand the feature.** Inspect the relevant code, tests, documentation,
   and `platformio.ini`. Identify the expected behavior and its edge cases.
   Ask for clarification when a hardware detail, server protocol, or behavioral
   decision is necessary and cannot be inferred safely.
2. **Implement the change.** Follow existing patterns and keep the change
   focused. Consider ESP32 memory, timing, connectivity, and error handling.
   Do not hard-code secrets or assume unconfirmed hardware configuration.
3. **Write or update tests.** Use the
   [`write-or-update-tests`](../write-or-update-tests/SKILL.md) skill to cover
   the requested behavior, relevant edge cases, and regressions.
4. **Update documentation.** Describe changes to setup, configuration, or
   user-visible behavior when applicable.
5. **Build.** Run `pio run -e esp32dev` from the project root. Examine and
   address build errors caused by the change.
6. **Run tests.** Run the relevant tests, including `pio test -e esp32dev`
   when tests, a suitable connected board, and authorization are available.
   PlatformIO device tests may upload firmware: ask for explicit approval
   before each upload attempt. Use the
   [`upload-firmware`](../upload-firmware/SKILL.md) skill for firmware uploads.
   If no tests exist or hardware is not available, report the gap rather than
   treating it as a pass.
7. **Check and repeat.** Review the changes and command output for errors and
   unintended effects. Fix issues caused by the change and repeat the affected
   build and tests until they pass or a blocker cannot be resolved. Report
   pre-existing failures and blockers clearly rather than hiding them.
8. **Report the outcome.** Summarize the feature and disclose any failed,
   skipped, or blocked validation. Commit changes only if the user explicitly
   requests a commit, and follow the repository's commit requirements.
