# Cura Remote SD plugin

The `ultimate-cura-plugin` directory contains a Cura 5 output-device plugin.
After slicing, select **Save to Remote-SD** from the arrow beside Cura's save
button, then click the save button. The plugin scans for
`_remote-sd._tcp.local.`, verifies `/api/device`, and asks which device to use
when more than one is found. Confirm the destination before uploading.

Uploads replace a file with the same name without a backup. SD access switches
the card away from the printer: **do not upload while printing**. Interrupted
or failed uploads can leave a partial file. Progress measures bytes sent; 100%
does not mean the device has finished. The success message means the device
returned an HTTP success response, not that SD-card writes were independently
verified: the current firmware does not report all card-write failures.
HTTP and mDNS are unencrypted and unauthenticated, so use a trusted network.

## Build and install

Requirements: Cura 5 (SDK 8), Python 3.7 or later with pip, and network access to
download the pinned dependencies. Build on the target operating system using a
Python interpreter compatible with the Python version bundled with that Cura
installation. Some platforms may build or download native dependency modules;
a zip built for one OS, architecture or Python version is not necessarily
portable to another.

From the repository root:

```sh
python3 ultimate-cura-plugin/build.py
```

The output is `ultimate-cura-plugin/dist/RemoteSD.zip`. Close Cura, then extract
the `RemoteSD` directory from the zip into Cura's user plugin directory:

- Linux: `${XDG_DATA_HOME:-~/.local/share}/cura/<major.minor>/plugins`
- Windows: `%APPDATA%\cura\<major.minor>\plugins`
- macOS: `~/Library/Application Support/cura/<major.minor>/plugins`

For example, Cura 5.4 uses `5.4`. Create `plugins` if needed. The installed layout must be
`plugins/RemoteSD/plugin.json`, not an extra nested `RemoteSD` directory.
Restart Cura and enable the plugin in the plugin manager if it is disabled.
Remove `plugins/RemoteSD` and restart Cura to uninstall it.

The zip is a manually installed plugin directory, not a signed Marketplace
`.curapackage`. Building it uses pip and includes the dependencies and their
license notices in the archive. For Marketplace distribution, follow
[UltiMaker's package instructions](https://github.com/Ultimaker/Cura/wiki/Creating-Packages-Plugins-For-The-Marketplace)
and meet its licensing and distribution requirements.

## Troubleshooting

The device must already be configured and connected to the same network as
Cura; setup mode does not advertise the discovery service. Multicast DNS can be
blocked by firewalls, Wi-Fi client isolation or network segmentation. A device
that disappears during the scan or cannot answer `/api/device` is not shown.
Try again when it is available. There is currently no manual-address fallback.
If discovery reports a missing Python dependency, rebuild and reinstall the
complete zip rather than copying the source directory alone.

## Development

Run the host tests from the repository root:

```sh
python3 -m unittest discover -s ultimate-cura-plugin/tests -v
```

The transport tests use a local HTTP server and simulated discovery. The Cura
UI integration tests require Cura/Uranium and PyQt6 in the Python environment;
they are skipped if those imports are unavailable. A live Cura UI and a real
Remote SD device are needed to verify end-to-end operation. No test uploads
firmware.
