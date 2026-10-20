# Qt / KDE runtime compatibility

[English](QT-RUNTIME.md) · [Italiano](QT-RUNTIME.it.md)

## Exact inputs

| Input | Previous runtime | Current runtime |
|---|---|---|
| MSYS2 MinGW64 Qt base | 5.15.18+kde+r109-6 | 5.15.19+kde+r96-1 |
| KDE qtbase revision | `bebdfd54917e25d1c100e6bd9f5dd53c2e645fd8` | `fbed962c3195ab3952fa54d40e012ad1a4fdc42b` |
| Upstream base tag | `v5.15.18-lts-lgpl` | `v5.15.19-lts-lgpl` |
| MSYS2 recipe revision | `82bcf47d0ae2621e1cd00fb82c8ee5db02b91955` | `a114954945ef1da8b3fbb12adcbac08f23628c1d` |

The 96 commits above the new upstream tag are patch-equivalent to 96 commits
in the previous KDE series (`git range-diff`). The 13 remaining old KDE commits
are incorporated in the new upstream base, not lost. Twelve have corresponding
upstream changes for D-Bus, item tooltips, native file dialogs, documentation,
generated header capitalization, line-edit animation cleanup, ODBC lengths,
RandR validation, QDom encoding, Windows temporary paths, data URLs and X11 input.
Eleven have identical stable patch IDs. The data-URL patch adds an extra upstream
test; its production source is identical at both KDE tips.
The thirteenth is an XML parser local-variable rename: both `qxmlstream.g` and
`qxmlstream_p.h` have identical Git blob IDs in the old tip, new tip and new base.
All 29 MSYS2 patch files are byte-identical, with unchanged build options and
dependencies. Recipe differences are the Qt version, release, source revision
and Git-source checksum.

The old package already included KDE backports for CVE-2025-30348/QDom and the
Windows GetTempPath2 change. They must not be advertised as newly introduced
protections. Qt 5.15.19 also brings upstream robustness fixes; bundled third-party
library updates do not necessarily affect this build, which dynamically links
several external libraries. See the [official Qt release notes](https://code.qt.io/cgit/qt/qtreleasenotes.git/tree/qt/5.15.19/release-note.md).

## Compatibility boundaries

All NetLink application modules and its internal libraries are rebuilt together
against the same 5.15.19 headers/import libraries. Windows QPA integration uses
the corresponding private headers. Qt platform/image plugins are taken from
that same package; mixed Qt versions are rejected during packaging.
The compiler, QCA/OpenSSL and other non-Qt runtimes remain unchanged.
The public-key algorithm (RSA/SHA-512), wire stream version (`Qt_5_5`), feature
identifiers, service identity, installation path, settings and key paths are
unchanged. Do not copy individual DLLs into an older installation.

Tests include old-runtime public RSA/wire fixtures, incorrect-signature rejection,
PEM/signing/OAEP, asynchronous transport, diagnostic TLS, discovery lifecycle,
Appello/profiles, GUI controls at three DPI scales and isolated installer
upgrade/uninstall/reinstall fixtures. Isolated installer fixtures simulate
service/firewall actions; they do not prove a privileged reboot round trip.
Native tray, remote control, WFP and classroom interoperability still require
the guided [Windows pilot](PILOT.md). This is not a complete security certification.

To reproduce the KDE comparison in a local qtbase repository containing both
revisions and tags:

```
git range-diff --no-patch v5.15.18-lts-lgpl..bebdfd54917e25d1c100e6bd9f5dd53c2e645fd8 v5.15.19-lts-lgpl..fbed962c3195ab3952fa54d40e012ad1a4fdc42b
```

Source archives, package/recipe checksums and exact versions are recorded in
`packaging/runtime-source-lock.json` and the release runtime-source manifest.
