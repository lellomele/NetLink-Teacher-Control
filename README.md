# NetLink Teacher Control

[English](README.md) · [Italiano](README.it.md)

NetLink is a Windows classroom-management suite based on **Veyon 4.7.5**.
It integrates remote control, WFP InternetGuard, student names, class–room profiles,
automatic client discovery and Companion diagnostics, without external plugins.

**Pre-release 1.0.0-alpha.30: use with awareness of its limitations.**

**[Download Windows x64 installer — latest release (alpha.30)](https://github.com/lellomele/NetLink-Teacher-Control/releases/download/netlink-1.0.0-alpha.30/NetLink-1.0.0-alpha.30-win64-setup.exe)**

## Features

- **Teacher Console:** computer thumbnails, remote control and Veyon commands.
  Ctrl+double-click a thumbnail to control that workstation; normal double-click
  keeps its configured action (remote view by default).
  The compact remote toolbar keeps the desktop visible, with a persistent pin,
  an option to keep controls open and right-click edge rotation.
- **Customizable appearance:** themes, colors and coordinated vector icon sets,
  high-resolution display support, descriptive tooltips and visible button-press feedback.
- **WFP InternetGuard:** block/release Internet on selected clients, with
  application/error feedback and filter expiry when renewal stops.
  All dialog rows are initially selected; commands affect highlighted rows only.
  InternetGuard, Lock and Demo recheck selected thumbnails for each request;
  an empty selection never targets the whole room. A confirmed active filter
  is marked `[web locked]` in white on red; inactive filters add no caption label.
- **Student names:** the student's self-declared name appears beside the Windows
  account and PC name. It does not verify identity or attendance.
- **Class–room profiles:** editable student/workstation assignments,
  Master-side JSON save/load, CSV export and separate preloaded/client-declared names.
- **Find clients:** WS-Discovery with IPv4-preferred automatic mode, IPv6 fallback,
  IPv4-only mode, known-computer filtering and grouping of the same discovery identity.
- **Simplified configuration:** Easy, Standard and Advanced views;
  reuse existing locations, computers, keys and access rules.
- **Status and diagnostics:** service tray summary and Companion PC/network
  analysis with distinct remote-control and discovery tests.
  The tray tooltip prefers the active network's IPv4; details retain the other
  listening addresses, VNC backend and forwarded-video information.
  Connected thumbnails show the client's VNC plugin, internal port and
  per-connection video observations over the existing authenticated channel.
  Both teacher and client need this feature; old, missing or expired replies
  are clearly marked unavailable. No additional ports are needed.
  An explicitly paired temporary channel transfers diagnostic reports.

## Installation and updates

1. Download the Windows x64 installer from
   [Releases](https://github.com/lellomele/NetLink-Teacher-Control/releases)
   and compare its SHA-256 with the attached checksum file. The alpha is unsigned.
2. Save a Configurator JSON export and back up authentication keys separately.
   The JSON does not contain private keys.
3. Close Teacher Console, Configurator and Companion. Install the full package
   as administrator, selecting **Master** for the teacher PC or **Client** for
   student PCs. Service and Configurator are required; Teacher Console is optional
   and selected for Master. Companion is optional, initially unchecked, and can
   be added later by rerunning setup. Updates retain installed choices.
   No prior uninstall or key regeneration is needed.
4. If files are in use or removal is pending, restart Windows and run the
   package again. Do not replace individual DLLs or skip files.

The reference platform is **64-bit Windows 10**. Dependencies are bundled.
The suite includes Qt 5.15.19 with KDE patches; no separate Qt installation is needed.

Existing Veyon configuration is reused. Keep the private key only on the
teacher PC and distribute the public key to clients.

The installer checks files before copying and preserves configuration and keys.
Uninstall can keep or remove settings, removes the entire program directory
and preserves external keys. It stops if keys are inside that directory:
move them to a protected location first.
Settings backups are in `C:\ProgramData\NetLink\Recovery`.

Full procedure: [installation, uninstall and recovery](docs/INSTALLATION.md).

## Quick start

- Configure keys/access rights in Configurator, keeping existing locations and PCs.
- Select workstations in the Console, then choose remote control,
  InternetGuard or student-name collection.
- In **Locations & computers → Class–room profiles…**, save/load assignments.
  A profile does not change Configurator network addresses.
- In **Find clients**, turn off the known-computer filter to see configured PCs
  too. Verify destinations before importing them.
  Import needs system-configuration write permission in the Console itself;
  an administrator Configurator does not elevate an already running Console.
- If installed, open **Start → NetLink → NetLink Companion**, choose the role and test:
  teacher-to-client remote control or client-to-teacher direct discovery.

[Feature and configuration guide](docs/USAGE.md) · [Test checklist](docs/PILOT.md)

## Operational limits

- Use trusted networks and non-administrator student accounts.
  Veyon 4.7.5 retains its security model: Companion TLS does not add
  general TLS protection to remote control.
- WFP feedback confirms installed filters, not a browsing test.
  Allowed destinations remain reachable on all ports; proxies/VPNs and other
  policies need testing. Filters expire after 90 seconds without renewal,
  but blocked native Windows calls may delay recovery.
- Discovery is unauthenticated. IPv4 preference uses recent announcements,
  not TCP tests or session failover. Multicast does not cross VLANs/subnets
  by itself. For InternetGuard use routable IPs or resolvable hostnames,
  not IPv6 link-local addresses with `%number`.
- Software input lock is not an inviolable kiosk: Windows protected screens
  remain accessible. Notifications do not prove which special shortcut was pressed.
- Profile JSON/CSV files and full Companion reports contain personal data:
  store them in protected folders and do not publish them.
- Interception and the binary DeskDupEngine DLL are not distributed.
  Classroom performance, IPv6-only networks and interoperability require
  deployment-specific testing; the audit is incomplete.

[Architecture, safeguards and outstanding verification](docs/AUDIT.md)

## Build

Clone with
`git clone --recurse-submodules https://github.com/lellomele/NetLink-Teacher-Control.git`.
The Windows build uses MSYS2 MinGW64, GCC, CMake, Ninja, Qt 5, QCA, OpenSSL,
OpenLDAP, LZO, libjpeg-turbo, libpng, zlib and NSIS.
These tools are for developers, not installer users.

[Build and packaging](docs/BUILD.md) · [Dependencies and licenses](docs/DEPENDENCIES.md).
Icon sources are in `branding/`. Full release source archives include submodules;
GitHub's automatically generated ZIP files do not.

## Copyright and license

NetLink: Copyright © 2025-26 prof. Ing. Raffaele Mele, for fork modifications.
Veyon © Tobias Junghans, Veyon Solutions and contributors.
This is not an official Veyon release. Original attributions are preserved.
Veyon code is GPL-2.0-or-later; see [COPYING](COPYING) and the bundled licenses.
