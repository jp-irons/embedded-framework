# Embedded TLS cert for HttpServer

`servercert.pem` and `prvtkey.pem` are baked into the firmware via
`EMBED_TXTFILES` (see `../CMakeLists.txt`) and used by `esp_https_server`.

## Why DNS SAN instead of an IP SAN

The device's IP is assigned by DHCP and can change at any time, so a cert
pinned to a specific IP would break after a lease renewal.  Instead, the cert
uses a DNS Subject Alternative Name (`DNS:esp32.local`) that matches the
hostname the `MdnsManager` advertises via mDNS.  Browsers that support
mDNS (Chrome, Firefox, Safari on macOS/iOS/Android) will resolve
`esp32.local` → current IP automatically, so the cert stays valid across
reboots regardless of what IP DHCP hands out.

If you change the mDNS hostname (via `FrameworkContext` constructor or
`WiFiContext::mdnsHostname`), regenerate this cert to match.

## Regenerate

```sh
openssl req -x509 -newkey rsa:2048 -nodes \
    -keyout prvtkey.pem -out servercert.pem \
    -days 3650 \
    -subj "/CN=esp32.local" \
    -addext "subjectAltName=DNS:esp32.local,DNS:esp32"
```

## Trusting the cert (one-time, per machine)

Browsers will warn on first visit because the cert is self-signed.
To silence the warning permanently:

- **macOS**: drag `servercert.pem` into Keychain Access → System,
  then double-click it and set *Trust* → *Always Trust*.
- **Windows**: double-click `servercert.pem` → Install Certificate →
  Local Machine → *Trusted Root Certification Authorities*.
- **Linux (Chrome/Firefox)**: import via browser settings under
  *Manage Certificates* → *Authorities*.
- **iOS**: AirDrop or email the cert, tap it to install, then enable
  it under *Settings → General → About → Certificate Trust Settings*.

**Do not use this cert in production.** For production, either provision
a per-device cert signed by a private CA (generated on first boot and
stored in NVS), or use a public CA via a domain you control.
