# Windows x64 build

The application base remains Veyon 4.7.5; all modules are rebuilt together.
Do not mix DLLs/plugins from upstream installers or another Qt/compiler ABI.

Install MSYS2 MinGW64 build packages:

```
pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-qt5-base mingw-w64-x86_64-qt5-tools mingw-w64-x86_64-qca-qt5 mingw-w64-x86_64-openssl mingw-w64-x86_64-openldap mingw-w64-x86_64-lzo2 mingw-w64-x86_64-libjpeg-turbo mingw-w64-x86_64-libpng mingw-w64-x86_64-zlib mingw-w64-x86_64-nsis
```

In PowerShell, add `C:\msys64\mingw64\bin` to PATH and configure:

```
cmake -S . -B build-school -G Ninja -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_SYSTEM_PROCESSOR=x86_64 -DCMAKE_C_COMPILER=C:/msys64/mingw64/bin/gcc.exe -DCMAKE_CXX_COMPILER=C:/msys64/mingw64/bin/g++.exe -DCMAKE_PREFIX_PATH=C:/msys64/mingw64 -DWITH_QT6=OFF -DWITH_BUILTIN_LIBVNC=ON -DWITH_TESTS=ON -DWITH_PCH=OFF -DWITH_UNITY_BUILD=OFF -DWITH_LTO=OFF -DWITH_TRANSLATIONS=OFF -DWITH_INTERCEPTION=OFF -DCMAKE_BUILD_TYPE=Release -DMINGW_PREFIX=C:/msys64/mingw64 -DMINGW_PLATFORM=win64 -DMINGW_TARGET=x86_64-w64-mingw32
cmake --build build-school -j 4
```

Run `ctest --test-dir build-school --output-on-failure` with `build-school/core` and the toolchain
binary directory in PATH. Tests must pass before packaging. For a release build,
configure a fresh `build-release` with the same options except `WITH_TESTS=OFF`;
this removes developer plugin search paths and runtime test hooks.

Generate icons if the vector design changes:
`powershell -NoProfile -ExecutionPolicy Bypass -File branding/render-icon.ps1`.
The icon rasterizer uses built-in Windows drawing APIs; the original SVG is retained.

Package from a fresh `dist` payload directory:
`powershell -NoProfile -ExecutionPolicy Bypass -File packaging/build-package.ps1 -Build build-release`.
This compiles existing translations without modifying TS sources, resolves DLL
imports recursively, preserves licenses and creates the NSIS installer. The legacy
`windows-binaries` target is not used on Windows.

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
