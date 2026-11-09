---
name: frontend-changes
description: Update the embedded Wi-Fi configuration frontend and its generated header.
---

# Frontend changes

1. Edit `frontend/setup.html`, keeping the setup portal endpoints and validation
   consistent with `lib/portal/setup_portal.cpp`.
2. Run `python3 build_views.py` to regenerate `lib/portal/views/setup_page.h`. Never edit
   the generated header directly.
3. Run `python3 build_views.py --check`, `node test/host/frontend_test.js`, the
   relevant host tests, and
   `pio run -e esp32dev`. Verify the generated page is served by the firmware;
   test it on the device when authorized hardware is available, otherwise report
   that hardware validation was not performed.
