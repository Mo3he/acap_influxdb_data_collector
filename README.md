# InfluxDB ACAP for Axis Cameras

[![Release](https://img.shields.io/github/v/release/Mo3he/acap_influxdb_data_collector?style=flat)](https://github.com/Mo3he/acap_influxdb_data_collector/releases)
[![License](https://img.shields.io/github/license/Mo3he/acap_influxdb_data_collector?style=flat)](LICENSE)
[![Build](https://github.com/Mo3he/acap_influxdb_data_collector/actions/workflows/build.yml/badge.svg)](https://github.com/Mo3he/acap_influxdb_data_collector/actions/workflows/build.yml)
[![Super-Linter](https://github.com/Mo3he/acap_influxdb_data_collector/actions/workflows/super-linter.yml/badge.svg)](https://github.com/Mo3he/acap_influxdb_data_collector/actions/workflows/super-linter.yml)
[![Sponsor](https://img.shields.io/badge/Sponsor%20My%20Work-EA4AAA?style=flat&logo=github&logoColor=white)](https://github.com/sponsors/Mo3he)
[![Buy Me A Coffee](https://img.shields.io/badge/Buy%20Me%20A%20Coffee-FFDD00?style=flat&logo=buy-me-a-coffee&logoColor=black)](https://www.buymeacoffee.com/mo3he)

An AXIS Camera Application Platform (ACAP) that collects metrics from Axis
devices and sends them to InfluxDB v2.

> **Disclaimer:** Independent, community-developed ACAP package. Not an official
> Axis product and not affiliated with, endorsed by, or supported by Axis
> Communications AB or InfluxData, Inc. Use at your own risk.

## Overview

The collector polls device metrics over VAPIX and writes them to InfluxDB v2
using the line protocol. Supported data types:

| Type | Description |
|---|---|
| CPU Usage | Device CPU load % |
| Memory Usage | RAM usage % |
| Network | Network throughput (kbps) |
| CPU Temperature | SoC temperature |
| Uptime | Device uptime in seconds |
| SD Card Usage | Storage usage % |
| Thermometry | Per-zone temperatures (thermal cameras) |
| Spot Temperature | Single spot temperature reading (thermal cameras) |
| Air Quality | CO2, temperature, humidity, VOC, NOx, AQI, PM1.0/2.5/4.0/10.0 (AXIS D6310) |
| People Counter | Occupancy, total in, total out (P8815-2 3D people counter) |

## Compatibility

- **AXIS OS:** 10.x through 13.
- **Verified on AXIS OS 13** (13.0.0, aarch64).
- **Architectures:** `aarch64` and `armv7hf`.
- **Requires:** an InfluxDB v2 instance.

## Installation

> **Signed packages:** Release `.eap` files are signed with the Axis ACAP
> signing service and install normally on AXIS OS 12.10 and later.
>
> **Upgrading from an earlier version?** The signing vendor changed, so
> installing over a previously installed unsigned build can fail with
> **"Couldn't install: app"** (device log: *"Vendor ID in manifest does not
> match the vendor ID of the previous version"*). To upgrade: back up your app
> configuration, **uninstall** the old version, then install the signed one.

Get the latest version from
[Releases](https://github.com/Mo3he/acap_influxdb_data_collector/releases) and
install the appropriate `.eap` file via the device web interface at:

```text
http://<device-ip>/#settings/apps
```

## Configuration

Open the ACAP settings page, enter your InfluxDB connection details (URL,
organization, bucket, API token), select the data types you want to collect, set
the poll interval, and enable collection.

<img width="670" height="893" alt="Settings screenshot" src="https://github.com/user-attachments/assets/43376481-3c61-43e2-9491-906c4141e08f" />

## Ports & security

The collector opens no inbound ports. It makes outbound connections only: to your
InfluxDB v2 instance (the URL you configure) and VAPIX calls to the local device.
The InfluxDB API token and other credentials are stored in the ACAP parameter
store.

## How it works

- **C backend:** collects data via VAPIX APIs and writes to InfluxDB using the
  v2 line protocol.
- **FastCGI HTTP endpoints:** `settings` (GET/POST), `test` (connection test),
  `status`, `debug`.
- **GLib main loop:** periodic collection via `g_timeout_add_seconds`.

## Build from source

Requires Docker or Podman.

```sh
./build.sh
```

This produces two `.eap` packages: `aarch64` for newer devices and `armv7hf`
for older ones.

## Links

- [InfluxDB](https://www.influxdata.com/)
- [Axis Communications](https://www.axis.com/)

## License

The packaging and app code in this repository is licensed under BSD 3-Clause (see
[LICENSE](LICENSE)). Bundled upstream components (`ACAP.c` and cJSON, both MIT) are
listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
