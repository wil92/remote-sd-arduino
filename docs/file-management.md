# File management

The configured device serves a file-management page. Select a file and start
the upload to see transfer progress; when the browser cannot determine the
transfer size, an indeterminate progress indicator is shown. The percentage
reflects bytes sent by the browser, so the device may still need time to
finish after it reaches 100%. The page is blocked during the upload and file
list update. Keep the tab open: closing or navigating away can interrupt the
upload. Browsers may also display a confirmation when leaving the page.
The upload button is enabled only when a file is selected. The selection is
cleared after each upload attempt, whether it succeeds or fails.

After a successful upload, the page fetches `GET /api/files` once and updates
the displayed list without reloading. An upload or list-update failure is
reported on the page. A failed or interrupted upload may leave a partial file
on the card; an existing file with the same name may have been replaced.
Deleting a file requires confirmation in the browser; cancelling does not
access the SD card.

## File list API

`GET /api/files` returns HTTP 200 with `Content-Type: application/json` and
`Cache-Control: no-store`. The body is an array, for example:

```json
[{"name":"example.gcode","size":12345}]
```

`size` is in bytes. In setup mode the endpoint returns HTTP 404. The response
is the device's cached list: it does not read the SD card or switch it away
from the printer. The cache is populated by the existing page load, upload, or
delete operations and can be stale if the card is changed externally.

Accessing the SD card to load the page when the cache is empty, upload, or
delete a file switches it away from the printer. Avoid these operations while
printing. HTTP is unauthenticated and unencrypted; use a trusted network.
