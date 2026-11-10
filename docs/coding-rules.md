# Coding Rules

These guidelines apply to the firmware, host-side tests, build scripts, and
embedded configuration frontend in this repository. Prefer consistency with
the surrounding module when extending existing code. When adding new code,
follow the conventions below.

## Project layout and file names

- Keep firmware startup and the main application loop in `src/`.
- Put reusable firmware modules in `lib/`, grouped by responsibility
  (for example, `lib/network/`, `lib/display/`, and `lib/flash/`).
- Put host-side tests and their hardware stubs in `test/host/`.
- Put detailed documentation in focused, topic-based files under `docs/`.
  Keep the root `README.md` concise as a project overview and documentation
  index, with links to relevant guides rather than duplicated details.
- Use `snake_case` for source and test file names, including `.h` and `.cpp`
  files. Use the conventional `README.md` name for readme documents.
  Keep a header and its implementation on the same base name, such as
  `network_manager.h` and `network_manager.cpp`.
- Keep implementation details in `.cpp` files where practical. Put shared
  declarations and small, hardware-independent templates or inline functions
  in headers.
- Do not edit generated files by hand. For the setup page, edit
  `frontend/setup.html` and regenerate `lib/portal/views/setup_page.h` with
  `python3 build_views.py`.

## C++ naming

- Use `PascalCase` for classes, structs, and enum types:
  `NetworkManager`, `Credentials`, `LoadResult`.
- Use `lowerCamelCase` for functions, methods, parameters, and local
  variables: `startScan()`, `passwordLength`, `credentials`.
- Use `snake_case` for namespaces and namespace-scoped constants, following
  the existing `wifi_settings::max_ssid_length` style.
- Use `UPPER_SNAKE_CASE` for preprocessor macros and build-time configuration
  symbols: `SETUP_AP_PASSWORD`, `RESET_CONFIG_PIN`.
- Use descriptive names that communicate units, limits, and ownership when
  relevant. Avoid unexplained abbreviations.
- Give private data members a trailing underscore, as in `connection_`.
- Use `enum class` for scoped enumerations.

## Formatting and source organization

- Indent C++ code with four spaces; do not use tabs.
- Put opening braces on the same line as declarations and control statements.
- Use braces for multi-line control-flow bodies. Keep short guard clauses
  concise, and preserve clear control flow.
- Keep lines and functions focused. Extract a helper when it makes a complex
  operation easier to understand or test.
- Use `#pragma once` in headers, matching the existing project style.
- In a `.cpp` file, include its matching header first. Group standard-library
  and Arduino/platform headers separately from project headers, with a blank
  line between groups.
- Make headers self-contained: include the declarations they use rather than
  relying on unrelated include order.
- Prefer `constexpr` for compile-time constants and `const` for values that
  should not change. Avoid mutable global state unless it is required by the
  application lifecycle.

## Design and embedded behavior

- Keep responsibilities separated by module. `src/main.cpp` coordinates
  startup and updates; reusable behavior belongs in the relevant `lib/`
  component.
- Prefer small, explicit interfaces and pass dependencies into a component
  when that makes it easier to test without hardware.
- Keep the firmware compatible with the project's Arduino framework and
  C++11 host-test builds. Do not rely on newer language features unless the
  toolchain configuration and all relevant builds support them.
- Be mindful of ESP32 RAM, flash, and stack use. Avoid unnecessary copies,
  large stack buffers, and dynamic allocation in frequently executed paths.
- Keep `loop()` and other frequently called code responsive. Prefer
  incremental or non-blocking operations for networking, scanning, and
  button handling; do not add long delays or waits without a clear need.
- Check results from hardware, storage, and network operations. On failure,
  report or propagate the error using the component's established behavior;
  do not silently pretend the operation succeeded.
- Keep hardware-specific pins, display setup, and build options in the
  existing configuration points. Confirm hardware details before changing
  assumptions about wiring.

## Input, credentials, and logging

- Validate external input at the boundary, including lengths and allowed
  values, before storing it or using it in hardware or network operations.
- Bound reads and writes to fixed-size buffers and account for null
  terminators. Prefer the existing validation helpers when they fit.
- Never commit Wi-Fi passwords, tokens, or other credentials. Use ignored
  local configuration files and provide safe example templates when needed.
- Do not log passwords, tokens, or other secrets. Logs should identify the
  operation and failure without exposing sensitive values.
- Treat data stored in flash and served over local Wi-Fi as potentially
  accessible to others; document relevant security limitations when adding
  such behavior.

## Tests and documentation

- Add or update host-side tests for behavior that can be tested without
  hardware. Keep tests deterministic and use the existing stubs for Arduino
  dependencies where appropriate.
- Build firmware changes with `pio run -e esp32dev`. Run applicable host tests
  as documented in [development.md](development.md).
- Do not upload firmware or run hardware-dependent tests without a suitable
  device and authorization.
- Update the relevant documentation when configuration, setup, or
  user-visible behavior changes. Keep examples consistent with the code.
