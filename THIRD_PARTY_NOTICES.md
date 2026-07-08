# Third-Party Notices

The InfluxDB Data Collector ACAP includes the following third-party components,
each under its own license. The ACAP's own code is licensed separately (see
`LICENSE`).

## Bundled source (compiled into the application binary)

### ACAP SDK wrapper (`app/ACAP.c`, `app/ACAP.h`)

- Copyright (c) 2025 Fred Juhlin
- Project: <https://github.com/pandosme/make_acap>
- License: MIT (see full text below)

### cJSON (`app/cJSON.c`, `app/cJSON.h`)

- Copyright (c) 2009-2017 Dave Gamble and cJSON contributors
- Project: <https://github.com/DaveGamble/cJSON>
- License: MIT (see full text below)

## Dynamically linked (provided by Axis OS / ACAP SDK, not redistributed here)

These libraries are linked at build time via `pkg-config` and provided by the
device platform; they are not bundled in the package:

- **libcurl** (curl license, MIT-style) — <https://curl.se/>
- **GLib / GIO** (LGPL-2.1-or-later) — <https://gitlab.gnome.org/GNOME/glib>
- **FastCGI** (`fcgi`, OpenMarket FastCGI license) — <https://fastcgi-archives.github.io/>
- **Axis ACAP SDK** libraries (`axevent`, `axparameter`, `vdostream`) — Axis Communications AB

---

## MIT License

The MIT License applies to the ACAP SDK wrapper and cJSON components listed above.

```text
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```
