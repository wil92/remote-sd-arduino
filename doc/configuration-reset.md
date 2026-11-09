# Resetting device configuration

The device stores its configured network credentials and setup status in
EEPROM. To return to the setup access point, press the active-low configuration
reset input (GPIO 20 in the current firmware) during startup or operation.
The firmware clears the setup-status flag, commits the change, and restarts.
On the next startup, the missing status flag causes the setup access point to
be started. The button is also checked while waiting for Wi-Fi and processing
web requests; hold it until the device restarts, since individual network or
storage operations may delay the check.

If clearing the EEPROM flag fails, the firmware logs an error and does not
restart, avoiding a reboot that would reload the old configuration. Release
and press the button again to retry.
