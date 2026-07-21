# Changelog

All notable changes to this project are documented here. Each version
links to its full release notes on GitHub.

The format is based on [Keep a Changelog](https://keepachangelog.com/).

## [1.1.2-Signed] - 2026-07-21 - InfluxDB Data Collector 1.1.2 (Signed)

- Packages are now signed with the Axis ACAP signing service and install
  normally on AXIS OS 12.10 and later.
- Vendor updated to `moshe@mohome.net` with the registered vendor ID.
- Upgrading from an earlier unsigned version can fail with "Couldn't
  install: app" (device log: "Vendor ID in manifest does not match the
  vendor ID of the previous version"). Back up your config, uninstall the
  old version, then install this one.

## [1.1.2] - 2026-07-07

## [1.1.1] - 2026-03-24

[1.1.2]: https://github.com/Mo3he/acap_influxdb_data_collector/releases/tag/v1.1.2
[1.1.1]: https://github.com/Mo3he/acap_influxdb_data_collector/releases/tag/v1.1.1
