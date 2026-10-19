# Dependencies and corresponding source

[English](DEPENDENCIES.md) · [Italiano](DEPENDENCIES.it.md)

NetLink (formerly Veyon School) uses the Veyon 4.7.5 application baseline:
`f24b48f5acea2a5728ba9f45c9d4df382d094b33` (GPL-2.0-or-later).
Copyright and license notices in inherited source files are retained.

The repository and the release source archive provide the application source,
build/packaging scripts and pinned internal libraries. UltraVNC corrections are
reproducibly applied from `patches-ultravnc.diff` during configuration.
The executable is a GPL-3.0 distribution where required by dependency combinations;
individual source files retain their original GPL-2.0-or-later grants. No restriction
on studying, modifying, rebuilding or replacing the dynamically linked libraries is imposed.

## Internal sources

| Component | Upstream | Pinned revision |
|---|---|---|
| UltraVNC | https://github.com/veyon/ultravnc | 6b734d107f1e0cc0d7254028a54d9a2eac9d81f6 + patch |
| LibVNCServer/Client | https://github.com/veyon/libvncserver | 27c2b554d7c5a9258f8049f77c5531d61fc34ea9 + patch |
| KLdap | https://invent.kde.org/pim/kldap | 23abaa68c3c43770e86998af38b3c8b2ffde4667 |
| KLdap Qt compatibility | https://invent.kde.org/pim/kldap | 4974359470669f8a2fcb017a2cbc6974aa8e6f3a |
| Qt HTTP server | https://github.com/veyon/qthttpserver | f80feff79a3d698ca3f23cfa240bd763d87cf0ba + patch |

Qt HTTP server uses its pinned http-parser submodule. The Git submodule tree records
the complete precise revisions, including the Linux-only libraries. See each
component's COPYING/LICENSE files for its license and copyright.

Interception integration is OFF in this installer. No interception DLL, kernel
driver or driver installer is distributed. The optional build source remains pinned
at 39eecbbc46a52e0402f783b872ef62b0254a896a; its own licensing must be checked before
enabling that optional build. The opaque legacy DeskDupEngine DLL is not distributed.

## Dynamically linked runtime

Windows x64 build uses MSYS2 MinGW64 packages. These are build dependencies, NOT
additional software that users have to install. Runtime DLLs are included.

| Component | Package version |
|---|---|
| Qt 5 base | 5.15.18+kde+r109-6 |
| QCA | 2.3.10-2 |
| OpenSSL | 3.6.2-2 |
| OpenLDAP | 2.6.12-1 |
| LZO | 2.10-3 |
| libjpeg-turbo | 3.1.4.1-3 |
| zlib | 1.3.2-2 |
| libpng | 1.6.58-1 |

Actual recursively resolved DLL names and SHA-256 hashes are supplied in
`package-manifest.json`. Installed package license texts accompany the installer.
The release also supplies a runtime-source archive for Qt, QCA, GLib, Graphite,
libiconv, gettext, LZO and GCC runtime, including the exact MSYS2 recipes/patches.
Its manifest pins source URLs, recipe commits and SHA-256 hashes. These library
sources are unmodified: application/vendor changes reside in the application
source archive. Other bundled libraries retain their permissive license texts.
MSYS2 dependency source recipes and patches are available at
https://github.com/msys2/MINGW-packages ; upstream source tarball URLs and versions
are specified by each package's PKGBUILD. Qt source changes are available at
https://invent.kde.org/qt/qt/qtbase (5.15 branch), QCA at
https://invent.kde.org/libraries/qca , OpenSSL at https://github.com/openssl/openssl ,
OpenLDAP at https://git.openldap.org/openldap/openldap .

This inventory is not a vulnerability-free certification. Some inherited libraries
predate their latest upstream releases; security backports and interoperability
validation remain a release gate (see AUDIT.md).
