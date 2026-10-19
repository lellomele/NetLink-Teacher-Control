# Installation, updates and recovery

[English](INSTALLATION.md) · [Italiano](INSTALLATION.it.md)

## Preparation

Use Windows 10 x64 and the complete installer from
[Releases](https://github.com/lellomele/NetLink-Teacher-Control/releases).
Compare SHA-256 checksums; alpha installers are unsigned.

Export Configurator JSON and back up keys separately: neither JSON nor registry
backups contain the private key. Keep the private key on the teacher PC with
restricted permissions, and the public key on clients.
Do not store keys or personal documents in the program directory.

## Installation and updates

1. Close Teacher Console, Configurator and Companion.
2. Run the installer as administrator on teacher and client PCs.
   No prior uninstall or regeneration of working keys is required.
3. Files are checked before copying. If files are busy or removal is pending,
   close setup, choose Restart in Windows and run the complete package again.
   DLLs cannot be skipped.
4. Verify version, service, tray icon, configuration, authentication and thumbnails.

Executables are `netlink-master.exe`, `netlink-configurator.exe`,
`netlink-service.exe`, `netlink-server.exe`, `netlink-worker.exe`,
`netlink-cli.exe`, `netlink-wcli.exe` and `netlink-companion.exe`.
Do not rename them manually. Setup handles the fork's legacy `veyon-*.exe`
files; libraries loaded into other processes may require a restart.

The directory `C:\Program Files\Veyon School`, configuration, key directories,
DLL names and `VeyonSchoolService` identity remain unchanged.
Setup updates suite-owned firewall rules; separately adjust custom rules,
scripts or policies that depend on old executable paths.

## Coexistence with original Veyon

The fork reuses Veyon configuration. Do not run both services together:
they use the same ports. When detected, the original service's automatic
startup is disabled with confirmation; its startup type can be restored
during uninstall. Uninstall does not automatically restart the original service.

The fork disables Interception and DeskDupEngine in shared configuration.
Uninstall does not reimport the previous configuration: use the original
Configurator and your backup to restore original settings.

## Backups and repair

The maintenance helper does not depend on an old installed Qt runtime.
Before updating, it saves registry settings and existing backups under
`C:\ProgramData\NetLink\Recovery`, protected for administrators and SYSTEM.
Repair an incomplete installation with the complete package:
do not copy individual DLLs or launch old tools from a damaged installation.

Busy files require restart; permission/read-only failures are reported separately.
Checks are not a transactional upgrade: a file may reopen after probing,
and a later failure has no complete automatic rollback.
Keep the message and backup, correct the cause and run the package again.

## Uninstall

Close applications and use Windows uninstall. Choose whether to keep settings
for reinstallation or remove them.

Both choices remove the entire `C:\Program Files\Veyon School` directory,
including manually added files. Busy files are removed at restart:
restart before reinstalling when requested.
External keys are never opened or deleted; if keys are inside the program
directory, uninstall stops so they can be moved first.

Settings cleanup covers suite-owned entries in 32/64-bit registry views and
loaded profiles. It preserves key paths, detected shared original-Veyon settings,
unloaded profiles, protected backups, external personal files and Windows-managed
data. Do not delete every entry containing “Veyon” or “NetLink”.

After uninstall and restart, verify the folder, service, Start shortcuts and
firewall rules pointing to the fork.
For a clean reinstall, install and reimport JSON in Configurator.
After uninstall with settings retention, configuration should already be present.

## Post-installation testing

Follow [PILOT.md](PILOT.md) on dedicated PCs with physical access.
Automated installer tests use isolated files/registry subtrees and simulate
service/firewall operations. They do not replace administrator installation,
restart and recovery tests in the actual environment.
