#!/usr/bin/env bash
# Legacy Veyon ABI: keep the same QCA/OpenLDAP packages as the released runtime.
# Their files remain in the official archive even if absent from live indexes.
# Hashes: https://packages.msys2.org/packages/mingw-w64-x86_64-qca-qt5
#         https://packages.msys2.org/packages/mingw-w64-x86_64-openldap
set -euo pipefail
directory="$(mktemp -d)"
packages=()
download() {
    local name="$1" expected="$2" file="$directory/$1"
    curl --fail --location --retry 3 --proto '=https' --proto-redir '=https' \
        "https://mirror.msys2.org/mingw/mingw64/$name" --output "$file"
    printf '%s  %s\n' "$expected" "$file" | sha256sum --check --status
    curl --fail --location --retry 3 --proto '=https' --proto-redir '=https' \
        "https://mirror.msys2.org/mingw/mingw64/$name.sig" --output "$file.sig"
    packages+=("$file")
}
download mingw-w64-x86_64-qca-qt5-2.3.10-2-any.pkg.tar.zst \
    94b9818564a644868573cf0b3e10d92ace793c937635b976fe81e717a51818ea
download mingw-w64-x86_64-openldap-2.6.12-1-any.pkg.tar.zst \
    5f5702423b3eb86c3181d830ce03de5ec683017b9a15ed9c6b20e94f55304074
# Keep pacman's signature and dependency validation. Never use --nodeps or
# disable SigLevel to get a superficially successful but incomplete build.
pacman --noconfirm -U "${packages[@]}"
