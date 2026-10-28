# Functional and resilience testing

[English](PILOT.md) · [Italiano](PILOT.it.md)

Use dedicated machines with physical access: one **teacher PC** and one or more
**student clients**, without personal device/network references.
Record version, OS, antivirus/firewall and each test result.
Do not share private keys, passwords or full reports without anonymization.

## Preparation and installation

1. Export Configurator JSON and separately back up keys. Verify installer checksums
   and install the full package as administrator on teacher/clients.
   If requested, restart and rerun it.
2. Keep existing keys: private only on the teacher, public on clients, with restricted
   permissions. Reuse rooms, computers and access rules.
3. Check configuration, service, visible/hidden tray icon and Start → NetLink.
   Do not run upstream and fork services together on the same ports.
4. Verify live thumbnails and authenticated remote control from the Master.
   TCP success is insufficient. Record initial time and CPU/memory load.
5. Use actual addresses or resolvable hostnames: DHCP can change IP addresses.
   Do not put documentation examples into the configuration.
6. Install **Master** on the teacher and **Client** on students. Check that Client
   has Configurator/Companion/service, no teacher EXE/shortcut, and remains remotely
   controllable. Test updates retaining the role and switching both ways, with
   configuration/keys unchanged. Repeat uninstall/reinstall for each role.

## Console, appearance and remote control

1. Test Configurator-selected Italian/English, light/dark themes,
   Organic/Outline/Spectrum/Classic sets and button sizes.
   Save must persist; Cancel must restore the previous choice.
2. Test 100/150/200% scaling, narrow windows and mixed-DPI monitors.
   Commands must remain accessible through overflow; check keyboard/focus,
   high contrast, remote pointing and title copyright.
3. Remote control must initially activate only at the top edge.
   Left-click pins/unpins; right-click rotates top → left → bottom → right
   exactly once, without forwarding commands to the client.
4. The pin must remain visible: fade when idle, restore on mouse/focus,
   and never fade in high contrast. Test menus, fullscreen, resize and disconnect.
5. Open/close InternetGuard, Roll Call and Find Clients at least 20 times,
   also without sending commands. Console/thumbnails must stay usable without
   full-screen black flicker, crashes or new Windows desktop errors.
6. A second click must reopen the same window; targets must remain those selected
   at first opening.

## Configurator and service icon

1. Switch Easy/Standard/Advanced without changing hidden values.
   Add/edit/remove groups and PCs without maximizing.
2. Change discovery, Master address and resources: Apply must enable, validate
   and persist after reopening. Invalid input must not cause partial saves.
3. Check hidden icons and HideTrayIcon. Summary/Service Status must be current,
   close without terminating the worker and not equate listening with connectivity.
4. On an isolated client with autostart enabled, update in place and reboot
   Windows twice: NetLink Service must be Automatic and Running without opening
   Configurator. Confirm the thumbnail reconnects. Run `netlink-cli.exe service
   verify` from an elevated terminal and retain its output.
5. On a disposable test PC only, verify unexpected process failure triggers
   bounded native recovery, while an explicit administrator stop remains stopped.
   Restore normal operation afterwards. A deliberately manual Configurator choice
   must survive an upgrade and must not be advertised as automatic startup.
   Record System / Service Control Manager events if a boot fails; simulated
   service-control tests do not establish reliability on physical client hardware.

## InternetGuard

1. On the test client, from an elevated terminal in the application directory,
   run `netlink-cli.exe school selftest`. Record results/error codes.
   It uses reserved test destinations, not a general Internet block.
2. Verify unrestricted browsing and start a download before blocking.
   Select only intended clients and use Block Internet.
3. Check feedback, thumbnail, new Internet connections, existing download, DNS
   and allowed resources. Test actual traffic, not just filter state.
   Closing/reopening the panel must not unblock.
4. Unblock: wait for confirmation and check browsing returns.
   Repeat with IPv6/QUIC, VPNs and the actual planned security policies.
5. Block, close the Master and wait at least 95 seconds: verify recovery.
   Repeat with teacher Wi-Fi loss, service restart and client reboot.
   No unwanted filters should remain.
6. Test reconnect/unblock during an in-flight request. No confirmation is not
   success; a new connection may need to wait for the former owner's lease expiry.

## Roll Call and class–room profiles

1. Request a name on the selected client. Test OK, main/numpad Enter, including
   after focusing Cancel. Exactly one response must arrive.
   Empty/whitespace names must not close with Enter; Esc/Cancel must cancel.
2. Verify `[Roll Call name] - Windows account - PC`, name background,
   tooltip, search and long names across supported themes/scales.
3. Create a profile with class, room and assignments; save JSON and reload
   after Master restart. Do not confuse it with a Configurator backup.
4. Request a different name: distinguish preloaded/declared without attendance
   confirmations or automatic file changes. Use Declared Names must copy only
   selected assignments.
5. Change class with a pending reply: the old reply must not modify the new profile.
   Disconnect/restart the client: clear the declaration, not the preloaded assignment.
6. Test malformed/incomplete JSON, duplicate UUIDs, oversized files and Configurator
   backups: reject without losing the active profile. New/unload must not delete files.
7. Change IP while retaining UUID: assignment remains, but Configurator does not
   automatically restore/correct addresses. Deleted/recreated PCs require reassignment.
8. Export CSV with Unicode, quotes and values beginning with `=`.
   Distinguish assignments/declarations; check responsiveness during I/O and closing.
9. Check two groups, highlight a PC in only one, leave another PC in that room
   unchecked and hide it with text search: JSON/CSV must include both room PCs,
   not the other group. Highlighting the root must give the same result.
   No highlighted row or an empty room must warn without writing a file.
10. Load with other groups present: confirm that only saved, available UUIDs are
    checked and that no tree row, group membership, address or key is replaced.
    Test Cancel, missing/recreated PCs, legacy JSON, pending Lock/Demo/InternetGuard,
    and a directory or feature change while the confirmation is open.

## Discovery and known-PC filter

Test bulk import with Ctrl/Shift, Select all and Clear selection. Confirm only
the selected PCs appear in the intended group after one save. During confirmation,
introduce an expired/conflicting/duplicate row or change the destination: no PC
may be imported. Refresh must retain selected identities without selecting new
arrivals; hidden/expired rows must drop from the visible selection. Test Cancel,
an empty selection with a current row, wrong ports, permission loss and save errors.

1. Automatic: one row for a dual-stack client, preferably IPv4; no teacher row
   on any adapter. Tooltips must show observed addresses.
   With three cloned system IDs and different names, expect three rows, each
   grouping its IPv4/IPv6 addresses. Also test cloned system ID + name but different
   physical MACs, and machines without usable permanent MACs. Route switches and
   enumeration order must not change identity when the installed physical-adapter
   set stays the same. Virtual adapters/current randomized MACs must not affect it.
   Hardware removal can change identity after a service restart, not keys/profiles.
   Allow stale pre-upgrade announcements to expire; contradictory names/ports
   sharing an identity must still block import.
2. For a client configured by IP/resolvable hostname, enable filtering: it must
   disappear from new results. Disable it: mark Already Configured, prevent duplicate import.
3. Test a client configured only by an IPv6 alias of the same identity, even if
   IPv4 is preferred in the row. DNS failure must retain results without blocking GUI.
4. Switch Automatic/IPv4 Only, close normally and reopen: preference must persist
   without changing Windows IPv6.
5. On an appropriate test network, an IPv6-only announcer must appear in Automatic
   but not IPv4 Only. Do not disable IPv4 on a remotely controlled PC without physical access.
6. Verify link-local IPv6 uses the receiving adapter index.
   Do not use it for InternetGuard resources; use routable IPs/resolvable hostnames.
7. Test new client, explicit import and removal from new results after saving.
   Master restart must show one entry. Discovery must not grant unauthenticated access.
8. Power off client: candidate expires within about 95 seconds.
   Close/reopen during DNS/announcements without crashes.
9. Test separate subnets/VLANs independently: configure DiscoveryMaster and allow
   TCP 3702 according to ACLs. Same-subnet tests do not validate routing/Wi-Fi isolation.
   Multicast does not independently cross VLANs; IPv6 multicast is link-local.
10. With two groups, highlight the second group or one of its PCs and import:
    confirm the displayed destination and parent group. With no highlighted group,
    a removed/renamed group or changed selection during confirmation, import must
    stop without falling back to another group or creating one. Cancel, save errors
    and duplicate results must not add PCs or overwrite unrelated settings.
11. Announce the same identity with different names and/or ports: the row and
    details must identify the exact disagreement and list observed addresses.
    Import stays blocked; matching dual-stack announcements are not a conflict.
    Check English/Italian and 100%, 150%, 200%, including markup-like remote names.

## Input lock and events

1. Lock; test Alt+Tab, Alt+F4, Ctrl+Esc, Ctrl+Shift+Esc and Windows key.
   Check rate-limited alerts without typed-text collection.
2. Test Win+L, Ctrl+Alt+Delete and UAC: expect session/protected-desktop notification,
   not a claim to identify a particular combination. The lock must return on desktop return.
3. Repeat events and Unlock: keyboard/mouse must recover without hangs/flicker.
   Events must stop; summary is limited to 200 rows.
4. Do not expect replay of disconnected events. Reconnect, Unlock/Lock and retry.
   Original Veyon clients do not produce extra messages; interoperability needs separate tests.

## Companion

1. Select role and distinguish TCP success, refusal, timeout, valid RFB and open
   non-RFB port. Teacher → client normally uses TCP 11100; client → teacher direct
   discovery uses TCP 3702 with Console open. Do not confuse directions.
2. Compare listeners, actual adapter profile and configured rules.
   An empty-user notification or TCP success does not prove authentication.
3. Receiver generates a temporary invitation; sender opens/pastes it, verifies
   destination and confirms diagnostic-data transfer. Wait for DELIVERED and
   RECEIVED/comparison. No repair must run.
4. Test modified/expired/reused invitation, cancel, network loss and app closure.
   No listener should remain at startup or after expiry.
5. Do not disable firewall for testing: any administrative permission must be
   limited to the necessary Companion/port/network. Share appropriate anonymized reports only.

## Uninstall and recovery

On dedicated systems, test in-place upgrade, busy files/reboot, clean install
and failure recovery. Follow [INSTALLATION.md](INSTALLATION.md);
do not run destructive tests on operational PCs.

Check both settings preservation/cleanup choices, folder/service/shortcut/fork-rule
removal, and external key preservation/permissions. Reinstall, restore JSON and
verify a real connection. Do not manually delete registry entries from broad searches.

## Load and results

Run prolonged sessions with multiple clients, offline PCs and reconnects.
Record latency, CPU/memory and reproducible steps; remove unnecessary data from
Companion reports. GUI delay logs appear after recovery and can include sleep/resume.

Two-PC tests do not establish large-class, IPv6-only, cross-subnet or interoperability
validation. Progress criteria/limits: [AUDIT.md](AUDIT.md).
