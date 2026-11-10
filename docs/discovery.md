# Network discovery

After a configured device connects to its Wi-Fi network, it advertises the
DNS-SD service `_remote-sd._tcp.local.` over mDNS on TCP port 80. The setup
access point does not advertise this service. Each device uses the hostname
`remote-sd-<mac>.local`, where `<mac>` is its Wi-Fi MAC address in lowercase
hexadecimal without separators, for example `remote-sd-001122334455.local`.
The hostname and device ID are stable as long as the Wi-Fi MAC address remains
the same; they are not secrets.

A Cura plugin or other client can browse `_remote-sd._tcp.local.`, resolve
each service's SRV target, port and addresses, and request `/api/device` from
that address and port. Use the `id` to deduplicate devices across addresses
and repeated announcements. Remove expired services and handle devices
disappearing or changing IP addresses. Apply a request timeout; a device may be
busy serving another request. A manual IP address is a useful fallback where
multicast DNS is unavailable or blocked between network segments.

For example, on a machine with Avahi:

```sh
avahi-browse -rt _remote-sd._tcp
curl http://remote-sd-001122334455.local/api/device
```

## Device information endpoint

`GET /api/device` returns HTTP 200 with `Content-Type: application/json`,
`Cache-Control: no-store`, and a connection-close-delimited body:

```json
{
  "type": "remote-sd",
  "id": "remote-sd-001122334455",
  "name": "Remote SD",
  "api_version": 1
}
```

`type` identifies this project, `id` identifies the device, `name` is the
display label saved during setup (default `Remote SD`), and `api_version` is
the integer version of this response contract. The endpoint does not access
the SD card or return Wi-Fi
credentials. In setup mode it returns HTTP 404. If mDNS initialization or
service registration fails, the serial log reports the failure and the HTTP
server remains available by IP address.

HTTP and mDNS are unauthenticated and unencrypted. Discovery identifies a
device but does not authenticate it; use this service only on a trusted local
network. mDNS multicast may be blocked by client isolation, firewalls, or
routers, and is not generally routed between subnets.
