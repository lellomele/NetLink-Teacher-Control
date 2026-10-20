# Windows x64 build

[English](BUILD.md) · [Italiano](BUILD.it.md)

The application base remains Veyon 4.7.5; all modules are rebuilt together.
Do not mix DLLs/plugins from upstream installers or another Qt/compiler ABI.

The Windows installer includes `netlink-maintenance.exe`, statically linked to
the C++ runtime and using Windows APIs only (no Qt). It backs up machine registry
configuration before stopping/replacing a damaged installation, verifies the
payload SHA-256 list before starting the CLI, and handles uninstall cleanup.
The native maintenance tests and the full NSIS round-trip fixture operate on
private build directories and exact HKCU test subtrees only. Service/firewall
operations are simulated in the fixture; they do not constitute a privileged
installation/reboot test. Run the PILOT.md sequence on isolated Windows PCs.

Install MSYS2 MinGW64 build packages:

```
pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-qt5-base mingw-w64-x86_64-qt5-tools mingw-w64-x86_64-qca-qt5 mingw-w64-x86_64-openssl mingw-w64-x86_64-openldap mingw-w64-x86_64-lzo2 mingw-w64-x86_64-libjpeg-turbo mingw-w64-x86_64-libpng mingw-w64-x86_64-zlib mingw-w64-x86_64-nsis
```

If the live MinGW64 index no longer offers `qca-qt5` or `openldap`, install the
other listed packages first, then run in the MSYS2 MinGW64 shell:
`bash .github/scripts/install-school-build-libraries.sh`.
The script retrieves the official QCA 2.3.10-2 and OpenLDAP 2.6.12-1 package
archives, checks pinned SHA-256 hashes and lets pacman validate signatures and
dependencies. This is a developer/CI step only; the installer already bundles
the required runtime. Do not mix UCRT64 and MinGW64 libraries in one build.

In PowerShell, add `C:\msys64\mingw64\bin` to PATH and configure:

```
cmake -S . -B build-school -G Ninja -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_SYSTEM_PROCESSOR=x86_64 -DCMAKE_C_COMPILER=C:/msys64/mingw64/bin/gcc.exe -DCMAKE_CXX_COMPILER=C:/msys64/mingw64/bin/g++.exe -DCMAKE_PREFIX_PATH=C:/msys64/mingw64 -DWITH_QT6=OFF -DWITH_BUILTIN_LIBVNC=ON -DWITH_TESTS=ON -DWITH_PCH=OFF -DWITH_UNITY_BUILD=OFF -DWITH_LTO=OFF -DWITH_TRANSLATIONS=OFF -DWITH_INTERCEPTION=OFF -DCMAKE_BUILD_TYPE=Release -DMINGW_PREFIX=C:/msys64/mingw64 -DMINGW_PLATFORM=win64 -DMINGW_TARGET=x86_64-w64-mingw32
cmake --build build-school -j 4
```

Run `ctest --test-dir build-school --output-on-failure` with `build-school/core` and the toolchain
binary directory in PATH. Tests must pass before packaging. For a release build,
configure a fresh `build-release` with the same options except `WITH_TESTS=OFF`;
this removes developer plugin search paths and runtime test hooks.

Discovery mode is stored with the Master's per-user settings under
`SchoolDiscovery/AddressMode` (`Automatic` or `IPv4Only`). Lifecycle tests inject
an in-memory store, simulated announcements and asynchronous DNS. The additional
IPv6 UDP test binds only ephemeral loopback sockets, explicitly skipping if IPv6
is unavailable. It does not open school firewall rules or probe configured PCs.
Set `VEYON_SCHOOL_SCHEMA_FIXTURES` to a build directory before running school-tests
to export the six SOAP samples, then run `tests/validate-discovery.ps1 -Fixtures`
with that directory. Dual-family metadata is covered by the same official schemas.

Generate icons if the vector design changes:
`powershell -NoProfile -ExecutionPolicy Bypass -File branding/render-icon.ps1`.
The icon rasterizer uses built-in Windows drawing APIs; the original SVG is retained.
The generated `netlink.ico` is embedded in all Windows executables and the
installer. Windows executable output names are `netlink-*.exe`; CMake target
identities and all DLL filenames remain unchanged. The protected `Veyon School`
installation directory, registry/key locations, feature/protocol identifiers
and `VeyonSchoolService` identity are retained. NetLinkIdentity centralizes local
launch and strict WFP peer paths. Legacy names are accepted only in diagnostic
reports and the installer upgrade selection, never as trusted WFP pipe aliases.
Non-Windows application output names are unchanged by this Windows alpha.

Appearance preferences are separate per-user values under
`HKCU\Software\Veyon\NetLinkAppearance`, never authentication/system settings.
The offscreen appearance tests accept `NETLINK_APPEARANCE_SCREENSHOTS` for PNG
output; on Windows offscreen rendering, set `QT_QPA_FONTDIR=C:\Windows\Fonts`
to make system fonts available. Neither variable is needed by the installed GUI.

The GUI restyling offers Organic/Outline/Spectrum/Classic icon preferences,
28/36/44 logical-pixel toolbar sizes and Configurator Easy/Standard/Advanced
views. These preferences use the same separate per-user appearance store;
switching views does not reset hidden system values. Original vector glyph
sources are QPainter paths in `core/src/NetLinkUi.cpp`, not external fonts or a
new SVG/icon library. The application branding sources/icons remain unchanged.
GUI DPI scaling is enabled only in Master/Configurator/Companion entry points,
not the server or screen-capture/input workers.

`netlink-ui-scale-1`, `netlink-ui-scale-1_5` and `netlink-ui-scale-2` test the real
directory page with an isolated JSON model, vector rendering, accessible native
tool buttons, view preservation, group removal confirmation and non-quitting tray
status dialogs. Set `NETLINK_UI_SCREENSHOTS` for PNG output. Console preview
images use real toolbar controls with synthetic cards, not a connected Master.
These offscreen tests do not replace native Windows tray/DPI/remote-input pilots.

`netlink-remote-toolbar-scale-1`, `-1_5` and `-2` exercise the actual compact
remote-control panel without constructing a VNC connection, initializing a
service, or reading authentication keys. Set `NETLINK_REMOTE_SCREENSHOTS` for PNG
previews. The view uses the same panel subclass in production; its existing VNC
command wiring remains in RemoteAccessWidget.cpp. Native keyboard focus/grab,
mixed-monitor DPI and real remote interaction remain pilot requirements.

`netlink-lock-notification-tests` uses inert lock widgets, isolated workers and
authenticated-channel fixtures. Shortcut classification is tested without
installing a real keyboard hook. Session events use Windows' built-in WTS APIs;
input-desktop checks are read-only and do not take over the protected desktop.
The production core links the system Wtsapi32 library; no new redistributable is
required. Consult `PILOT.md` and `AUDIT.md` for native tests and beta gates.

`branding/render-companion-icon.ps1` generates the Companion icon from the retained
`companion.svg` design using Windows drawing APIs. Companion is built as
`companion/netlink-companion.exe`, with existing Qt/core runtime dependencies and
Windows system APIs only. The installer adds a third Start-menu shortcut.
Its collector runs in a separate, read-only child process, bounded by the GUI to
30 seconds/2 MiB; no VeyonCore instance or service/plugin initialization is used.
The `--collect` switch emits a full local report to standard output, including
identifiers: do not redirect/share it without appropriate data handling.
`--version` emits the application version without collecting any information.

Package from a fresh `dist` payload directory:
`powershell -NoProfile -ExecutionPolicy Bypass -File packaging/build-package.ps1 -Build build-release`.
This compiles existing translations without modifying TS sources, resolves DLL
imports recursively, preserves licenses and creates the NSIS installer. The legacy
`windows-binaries` target is not used on Windows.

The installer runs its extracted Qt-free helper, not an old installed CLI, before
replacing files. It checks pending removals, backs up settings, stops the owned
service without unregistering it, and probes the destinations from the fresh
payload manifest for exclusive write access without changing file contents.
Only successful readiness checks permit unregistration and copying. A busy file
exits 3010 with an explicit restart request; permission errors fail separately.
There is no forced reboot/process termination, and no Ignore option for copying.
The check cannot prevent a file from being reopened afterwards and is not a full
transactional rollback. Current files are packaged from an explicit eight-EXE
allowlist, ignoring stale build outputs.
The installer offers Master/Client roles, persisted for upgrades. Client omits
only the Master EXE/shortcut. Separate full/client SHA-256 manifests are generated;
only the selected one is installed as payload.sha256. The full preflight plan
also probes an existing Master EXE before Client removes it. Isolated round-trip
tests cover role transitions, persistence, invalid arguments and shortcut targets.
Only the seven known legacy EXEs in the fixed fork directory are retired after
the new service starts. Reinstalling alpha.9 or later prefers its current CLI.
The NSIS fixture tests that same selection/retirement code on inert files in a
private directory, without real service, registry or reboot-state operations.
It does not establish successful privileged installation/upgrade/uninstall;
those still require the guided Windows pilot.

Internal UltraVNC changes are stored as `patches-ultravnc.diff`, applied only to the
pinned revision. Configuration fails if neither pristine nor exact patched source
is present. After changing a vendor source, regenerate and inspect the patch before
committing. The submodule reference itself remains unchanged.

Full source archives must contain submodules and the vendor patch; GitHub's default
ZIP does not include submodule contents. Release DLL import checks and unit tests do
not replace clean-VM installer tests or WFP/network pilot tests.

After committing the release revision, `packaging/build-source-package.ps1`
creates the application source archive with every pinned submodule. Vendor sources
are pristine; CMake applies the three supplied patches. Legacy opaque binary
files are excluded from Git source exports.

`packaging/build-runtime-sources.ps1` collects the exact copyleft runtime sources
and package recipes. It requires GitHub CLI access and the original MSYS2 package
archives in the build machine's cache. Recipes are matched against BUILDINFO
SHA-256 hashes, without executing downloaded scripts. The runtime-source archive
is a separate release asset; none of these development tools is needed on clients.
