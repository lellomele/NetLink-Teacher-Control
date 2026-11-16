[Italiano](README.it.md) | [English](README.md)

# NetLink Service Check

Portable Windows x64 utility to inspect the local NetLink service, configure automatic startup and explicitly start it. Version 1.1.0, Italian interface. No Qt installation is required.

## Use

Run `NetLink-Service-Check.exe` and approve administrator access. The initial analysis is read-only. Green rows indicate positive checks/information, yellow rows need attention, and red rows indicate problems or blocked changes. Hover or double-click for the full explanation.

Choose automatic startup with or without delay, then confirm **Imposta avvio…**. Optional recovery restarts the service after 10, 30 and 60 seconds, then stops retrying; the failure counter resets after 24 hours without failures. It never reboots the PC or launches external commands. **Avvia servizio…** starts it now without changing startup settings. **Salva rapporto…** exports text or JSON to compare before/after changes and after reboot. Optional 15-second refresh only reads status and does not repair in the background.

## Interpretation and limits

**Boot check:** select 3 minutes, 5 minutes or Disabled, then **Imposta controllo…**.
This does not delay normal Windows startup. It installs the native helper under
`Program Files\NetLink Service Tools` and registers the **NetLink Service Startup Check**
SYSTEM task, without a logged-in user. Running services are left alone; stopped
Automatic services receive at most three start requests, with two-minute retry pauses.
Manual/Disabled and installer maintenance are respected. Windows may execute it later
on a busy PC. Existing NetLink versions in `Program Files\Veyon School` are supported;
the check does not replace or fix defects in an old service binary. Reports are in
`C:\ProgramData\NetLink\StartupCheck`. Disabling removes the task but keeps the protected helper.

Manual/disabled startup differs from an automatic service that fails or stops during boot. Successful manual startup does not prove the reason: inspect timeouts, dependencies, account logon and subsequent failures. A delayed start may suit affected computers better. Event 7040 identifies a startup-type change, not necessarily its author. Error 1077 means startup has not been attempted since the last boot.

Windows controls startup order through service groups and dependencies. The utility does not change global ordering, CPU priority, unrelated services, dependencies, accounts or global timeouts. Non-delayed startup avoids the delayed queue but cannot guarantee precedence over other automatic services. Reboot verification must be performed by the user after saving work.

Only NetLink's technical service name `VeyonSchoolService` is writable, with a recognized executable in `Program Files\Veyon School` or `Program Files\NetLink School` and no reparse points along the path. Other/custom installations remain read-only. File metadata is not proof of publisher authenticity.

Every write/start requires a parameter backup under `%LOCALAPPDATA%\NetLink\ServiceCheck\Backups`, restricted to the current user, Administrators and SYSTEM. Backup failure prevents the action. Backups are reference copies, not automatically imported. SCM changes are not one atomic transaction: partial failure is reported and reread without blind rollback. No configuration, authentication keys or firewall rules are changed. No missing service is created, process terminated or network port opened.

The System log search covers seven days, at most 1,500 SCM events and 30 matching records. History is evidence, not proof of the current root cause. Missing/unreadable logs are reported. Running service status does not prove working remote access, video or discovery. Reports contain no passwords/keys but may contain computer/account names and paths: review before sharing.

## Requirements and build

Windows 10/11 x64, .NET Framework 4.8 and administrator consent. The portable executable is unsigned. Run `build.ps1` in PowerShell to compile the native helper with MSYS2 MinGW-w64 (`C:/msys64/mingw64`), then compile the GUI with Windows' .NET Framework compiler and run isolated tests. Alternatively pass `-NativeHelper` pointing to the matching helper built from the suite sources. Icon SVG/PNG/ICO sources are in `branding`.

See Microsoft's [startup order](https://learn.microsoft.com/en-us/windows/win32/services/automatically-starting-services), [delayed startup](https://learn.microsoft.com/en-us/windows/win32/api/winsvc/ns-winsvc-service_delayed_auto_start_info) and [failure recovery](https://learn.microsoft.com/en-us/windows/win32/api/winsvc/ns-winsvc-service_failure_actionsw) documentation.

© 2026 prof. Ing. Raffaele Mele. GPL-2.0-or-later; see `LICENSE.txt`.
