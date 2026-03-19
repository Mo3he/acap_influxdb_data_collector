# ACAP InfluxDB Data Collector

An AXIS Camera Application Platform (ACAP) that collects metrics from Axis devices and sends them to InfluxDB v2.

## Supported data types

| Type | Description | Measurement |
|---|---|---|
| CPU Usage | Device CPU load % | `device_metrics` |
| Memory Usage | RAM usage % | `device_metrics` |
| Network | Network throughput (kbps) | `device_metrics` |
| CPU Temperature | SoC temperature | `device_metrics` |
| Uptime | Device uptime in seconds | `device_metrics` |
| SD Card Usage | Storage usage % | `device_metrics` |
| Thermometry | Per-zone temperatures (thermal cameras) | `thermal_zones` |
| Air Quality | CO₂, temperature, humidity, VOC, NOx, AQI, PM1.0/2.5/4.0/10.0 (AXIS D6310) | `air_quality` |

## Requirements

- Docker (for building)
- An AXIS device running firmware 10.x or later
- InfluxDB v2 instance

## Building

```sh
./build.sh
```

This produces two `.eap` packages — `aarch64` for newer devices and `armv7hf` for older ones.

## Installation

Install the appropriate `.eap` file via the device web interface at:

```
http://<device-ip>/#settings/apps
```

## Configuration

Open the ACAP settings page, enter your InfluxDB connection details (URL, organisation, bucket, API token), select the data types you want to collect, set the poll interval, and enable collection.

## Architecture

- **C backend** — collects data via VAPIX APIs and writes to InfluxDB using the v2 line protocol
- **FastCGI HTTP endpoints** — `settings` (GET/POST), `test` (connection test), `status`, `debug`
- **GLib main loop** — periodic collection via `g_timeout_add_seconds`
