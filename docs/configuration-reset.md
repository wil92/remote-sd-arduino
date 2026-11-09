# Resetting device configuration

The device stores its configured network credentials and setup status in
EEPROM. To return to the setup access point, hold the active-low configuration
reset input (GPIO 20 in the current firmware) low while the device starts.
Firmware clears the setup-status flag, commits the change, and restarts. On
the next startup, the missing status flag causes the setup access point to be
started.

If clearing the EEPROM flag fails, the firmware logs an error and does not
restart, avoiding a reboot that would reload the old configuration.
