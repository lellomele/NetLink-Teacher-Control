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
   Choose **Master** on the teacher PC or **Client** on student PCs.
3. Files are checked before copying. If files are busy or removal is pending,
   close setup, choose Restart in Windows and run the complete package again.
   DLLs cannot be skipped.
4. Verify version, service, tray icon, configuration, authentication and thumbnails.

### Master / Client

One installer supports both roles and a native checkbox list, in this order:

1. **NetLink Service:** required, checked and not deselectable.
2. **NetLink Configurator:** required, checked and not deselectable.
3. **NetLink Teacher Console:** optional, selected for Master, not for Client.
4. **NetLink Companion:** optional, unchecked on a fresh installation.

Checking Teacher Console selects the Master role; unchecking it selects Client.
Client does not install `netlink-master.exe` or its Start shortcut. Shared
libraries/plugins remain necessary; this is an installation choice, not a new
authorization boundary. Start application labels all begin with **NetLink**.

Fresh installations default to Client. Updates retain the last installed role;
older full installations with the teacher executable default to Master.
Updates also retain the Companion selection, including detection of older
installations without component metadata. To add Companion later, rerun setup
and check it; no uninstall is needed. Unchecking it removes its executable and
shortcut, not exported diagnostic reports, configuration or authentication keys.
To switch roles, rerun the installer and select the other option. Choosing Client
also removes a previously installed Console and its shortcut. Close Console first;
busy files require a restart before any copying or removal. Configuration, key
files and their permissions are not changed by the role selection. Selecting Client
does not remove any existing private key: keep private keys only on authorized Masters.

For unattended installation use `/S /Role=Client` or `/S /Role=Master`.
Without `/Role=`, setup retains the role (Client on fresh installs).
Add `/Companion=1` to install Companion or `/Companion=0` to omit/remove it.
Without that option, its installed selection is retained (off on a fresh install).
Invalid role/Companion values are rejected before maintenance.
`/Master=hostname` is a
separate option for the direct discovery destination; it does not select the role.
Uninstall and recovery remain the same for both roles.

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

The question is **“Do you also want to delete NetLink settings?”**:

- **Yes:** uninstall and delete suite-owned settings; reconfigure or import a
  backup after reinstalling.
- **No:** uninstall but retain settings for reinstallation (recommended).
- **Cancel:** do not uninstall.

Both Yes and No remove the program directory and preserve external authentication
keys and their configured paths. Detected shared original-Veyon settings are retained.

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
