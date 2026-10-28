# Features and configuration

[English](USAGE.md) · [Italiano](USAGE.it.md)

## Teacher Console and remote control

The Console reuses Configurator workstations. A real connection and updating
thumbnail establish authenticated exchange; a successful TCP test is insufficient.
New features require compatible NetLink clients.

Hover a connected thumbnail for **Client service diagnostics**: the VNC plugin,
internal backend port, VNC handshake status and time since a video frame was
forwarded on that exact connection. Other sessions' frames are not counted.
Both the Console and client must support this extension (alpha.29 or newer).
It reuses the authenticated NetLink connection and opens no new ports.
Reports refresh approximately every 15 seconds; missing replies, invalid data
and disconnects invalidate the display, with slower retries for older clients.
An old, offline or not-yet-connected client is shown as diagnostics unavailable,
not as a VNC failure. A connection that cannot complete its initial handshake
still requires local service details: diagnostics do not bypass that handshake.
Forwarded frames do not prove display on the Master; a static desktop can
legitimately send no new frames. No video probe or configuration change is made.

Double-click a thumbnail for the configured action (remote view by default).
**Ctrl+double-click** starts remote control of that workstation only, even if
other thumbnails were selected. Existing permissions and disabled-feature rules
still apply; this shortcut does not grant additional access.

The compact remote menu leaves the desktop visible. Each session starts on the
top edge. Its pin stays visible and fades after inactivity; hover restores it.
Left-click pins/unpins the toolbar. Right-click on the pin or toolbar rotates
top → left → bottom → right. Only one edge is active.
Menus and keyboard focus keep controls accessible.

Appearance, colors, sizes and icon sets are personal preferences. Remote controls
follow the Console's set. High contrast disables pin fading.
Choose language in Configurator, apply and restart applications.
Windows dialogs may still use the system language.

Panel buttons, including class–room profiles, show a transient pressed state
for mouse clicks and the Space key. Release restores the normal/checked state;
there is no animation delay or additional command. Windows high contrast keeps
native accessible button rendering.

**Lock and Demo** read the current thumbnail selection on every request, from
both the toolbar and context menu. Select at least one workstation: no selection
never means all computers. A mixed group starts the requested mode on that group;
when all selected workstations already have that mode, the button stops it only
for them. Other recipients keep receiving Demo. To share a student's screen,
select the source and recipients, then choose the source from that group.
Changing the source is refused while unselected PCs receive a different Demo.

## Configurator

- **Easy:** access, keys, essential service settings, groups/PCs and discovery.
- **Standard/Advanced:** access to additional settings.
- Switching views must not change hidden values.
- Group/PC add/remove controls stay above the tables without maximizing.

For key authentication, restrict the private key to authorized teachers and
install the public key on clients. Never distribute the private key.
With multiple private keys, verify selection and permissions:
`VEYON_AUTH_KEY_NAME` permits explicit selection.
A test requiring a private key does not validate a client's public key alone.

## WFP InternetGuard

1. Select clients in the Console and open InternetGuard. All listed rows start selected.
   Use Ctrl-click/Shift-click to change the selection; **Block Internet**, **Unblock**
   and **Check status** affect highlighted rows only. No selection disables commands.
   The panel follows the Console's current thumbnail selection and rechecks it
   immediately before every command, even when already open. Row selection can
   further restrict those recipients; it never adds unselected thumbnails.
2. Read each client's confirmation, error or missing response.
3. Use **Unblock** to release filters. Closing the panel does not unblock.
4. **Check status** queries the service; it does not test browsing.

A confirmed active filter adds **[web locked]** to the thumbnail in white on
opaque red. A confirmed release removes the label; pending/error details remain
in the tooltip and InternetGuard panel. This reports filter state, not a browsing test.

The Master renews every 20 seconds; expected expiry is 90 seconds without renewal.
Allowed destinations reuse selected PCs and the Master's actual address.
The client detects DNS/DHCP on active adapters. Add extra IP/CIDR resources
in Configurator's NetLink page only when needed.
Do not authorize entire subnets without a reason.

Allowed destinations remain reachable on all ports: proxies, VPNs and forwarding
services can provide external access. Another connection's policy cannot be
replaced; a reconnect may have to await its previous lease expiry.
If Windows does not confirm WFP-session closure, state stays unknown and
cleanup is retried; a false successful unblock is not reported.
Disconnect invalidates previous confirmations. Closing the Console stops renewals
and sends release commands; an unconfirmed response is not proof of release.

Limits: 256 clients per operation, 255 destinations and 128 extra resources.
Do not use scoped link-local IPv6 (`%number`) as InternetGuard destinations;
prefer routable IPs or resolvable hostnames.

## Student names and class–room profiles

**Attendance → Request names** opens the dialog on selected clients.
Students submit with OK or Enter. Captions show
`[attendance name] - Windows account - PC`, highlighting only the name.
There is no attendance-confirmation mechanism or verified identity.

In **Locations & computers → Class–room profiles…**:

1. Highlight a room/group row or one of its PC rows in **Locations & computers**.
   The highlighted row chooses the group, not the checkboxes or thumbnail selection.
   **Use highlighted group** fills the editor with every configured PC in that group,
   including unchecked PCs and PCs hidden by the text search. Set class, room and names.
2. **Save JSON** saves all PCs in that group, not other checked groups. **Export CSV**
   uses the same scope. Existing editable assignments are kept; PCs not yet assigned
   use their current attendance name when available. **Use declared names** copies
   newer declarations explicitly. Clearing a name never removes the configured PC.
3. **Load JSON** previews the class, group and number of recognized PCs. Confirm to
   check only saved PCs and restore names. Other PCs are unchecked, not deleted:
   the tree, group memberships, current addresses, keys and access rules are unchanged.
   Missing/hidden PCs are listed but not created or matched by a reused IP/name.
   If no saved PC is available, nothing changes. End Lock, Demo and InternetGuard,
   including unconfirmed unblocks, before loading. Cancel keeps the active profile.
   Older class–room JSON files are readable; new grouped files need alpha.31 or later.
   This is not Configurator JSON, and profiles do not load automatically at startup.
4. Edit an assignment or **Use declared names** on selected rows.
   Replies never rewrite the file automatically: save explicitly.
5. **Export CSV** creates a readable copy of assignments and declarations,
   not a reloadable profile.
6. **End requests and clear declared names** keeps the profile;
   **New / unload profile** clears active labels, not files on disk.

Preloaded names differ from current declarations. Disconnect clears declarations,
not assignments. Matching uses workstation UUID, not IP.
With DHCP, use resolvable hostnames or update Configurator; profiles do not fix
old IPs. Deleted/recreated PCs need reassignment.
JSON/CSV files contain cleartext personal data: keep them in protected folders
outside the installation directory. A network write may finish after panel closure.

## Find clients

**Automatic — prefer IPv4** uses a recent IPv4 announcement, otherwise routable
IPv6 and then link-local. **IPv4 only** hides IPv6-only results.
This personal preference is saved on normal Console exit; it does not disable
Windows IPv6 or perform TCP failover.

One row groups observed addresses for a discovery identity; its tooltip lists them.
If announcements disagree, the status distinguishes different names, NetLink ports
or both. Select the row to see each observed name/address/port in the detail panel;
hovering the row shows the same explanation. Automatic import remains blocked.
Multiple IPv4/IPv6 addresses with matching names and ports are not a conflict.
Metadata addresses alone are not evidence. The teacher is excluded on every adapter.
Contradictory announcements stay visible and cannot be imported automatically.
Discovery identities combine the system ID, normalized computer name and, on
Windows, available permanent MACs of physical Ethernet/Wi-Fi adapters.
Installed disconnected adapters are included; virtual/filter interfaces and
unusable MACs are ignored. The active route, IP address and current/randomized
MAC are not identity inputs. MACs are hashed locally, not added to announcements
in cleartext. Without usable MACs, system ID + name still distinguish cloned PCs
with different names. This is not authentication: identical/spoofed inputs can collide.

Renaming a PC, reinstalling Windows or adding/removing/replacing an included
adapter can change its discovery identity. This does not change authentication
keys, configured workstation UUIDs or class profiles. Identity is fixed for each
running discovery agent; restart the service after such changes.
Update affected clients, not just the Master. Older clients keep their former
announcements; allow about 95 seconds for stale records to expire after upgrading.
No Windows machine identifier or adapter setting is modified.

**Hide computers already in Locations & computers** matches observed addresses
against configured IPs and DNS A/AAAA answers across all locations.
Unresolved names leave results visible; an old DHCP IP is not stable identity.
Disable the filter to see known PCs too.

Verify the destination before import. Configuration write rights and Master
restart are required to display a new PC. Before importing, highlight the destination
group or one of its PC rows in **Locations & computers**: checkboxes and selected
thumbnails do not choose this group. Discovery shows the destination and confirms
it together with the address and port. No highlighted row means no import; it
never falls back to the first group or creates a group automatically. If the group
is changed, renamed or removed while confirmation is open, retry the import.
Import does not alter keys/access rules.
These rights belong to the Console process: an administrator Configurator does
not elevate it. A changed, expired or conflicting announcement must be selected again.
A link-local address retains the teacher's receiving-interface index, applies
only on that link and may need updating after an adapter change.

### Network and ports

- Same link: WS-Discovery multicast on UDP 3702.
- Other subnets: set the teacher's hostname/IP in client
  `School/DiscoveryMaster`. Direct announcements use HTTP on TCP 3702;
  routing/ACLs must allow it. Setup also accepts `/Master=PC-NAME`.
- Managed discovery exceptions use Private/Domain profiles.
  Multicast does not independently traverse VLANs/subnets; IPv6 multicast is link-local.

## Input lock and service status

Software lock intercepts ordinary shortcuts and displays a cover.
Protected screens remain accessible; the cover is restored on return.
Events describe session/desktop observations, not proof of Ctrl+Alt+Del.
Very brief transitions may be missed. Notifications are grouped roughly every
two seconds, sent to the most recent initiating Master and kept in a volatile
summary of at most 200 lines. No typed text is collected.
After reconnecting, repeat Unlock/Lock.

The service tray icon gives a local summary and opens details.
Its short tooltip shows one address, preferring the IPv4 used by an authenticated
NetLink connection. Otherwise it uses the Windows routing choice, then active
Ethernet/Wi-Fi addresses. IPv6 remains visible when no IPv4 is available.
All other local addresses stay in the details; they are not connected clients
and listing them is not proof of remote reachability. No network probe is sent
to choose the address.
The Worker icon displays the local Server listener, not a separate remote-control
listener owned by the Worker. Details list the bind and interface addresses,
selected VNC backend/internal port and forwarded-frame observations per connection.
The internal TCP check is asynchronous and loopback-only; it does not test VNC
authentication. An authorized control connection without a frame is not working video.
Also check Windows hidden icons. “Listening” does not establish reachability,
authentication, discovery or successful Internet blocking.

### Windows startup and recovery

Enable **Autostart service** in Configurator → Service for startup after a reboot.
The installer preserves this setting, including an intentional manual choice;
it does not silently override it. With autostart enabled, Windows startup is
Automatic, not delayed. Installation verifies the running state, startup mode
and recovery settings, and reports an incomplete installation if verification fails.

Windows retries an unexpected service failure after 10, 30 and 60 seconds,
then stops retrying until the failure count resets after a failure-free day.
An ordinary administrator stop or Windows shutdown is not a failure. There is
no scheduled watchdog, automatic PC reboot or guarantee against persistent
startup errors. Restart Windows after updating to activate all recovery settings.

If a client stays offline after a reboot, first check NetLink Service in Windows
Services. If it is stopped, inspect Companion → Analyze PC: its full report
includes the native startup mode, exit codes, dependencies and recovery status.
Also check Event Viewer → Windows Logs → System → Service Control Manager at
the boot time. A listening/firewall test cannot explain a stopped service.
From an elevated terminal in the installation directory,
`netlink-cli.exe service verify` performs the same read-only startup check.

## Companion

If installed, open **Start → NetLink → NetLink Companion**, choose a role and **Analyze PC**.
Communication tests distinguish:

- **Remote control:** teacher → client, normally TCP 11100 with the client
  service running; optionally verifies the initial RFB greeting.
- **Direct discovery:** client → teacher, TCP 3702 with the Console open.
  Tests TCP opening only, not multicast or application announcements.
- **Custom TCP:** explicit destination/port, including loopback for local/remote comparison.

Tests take at most eight seconds. Full reports retain up to 20 session results
and identifiers; reduced reports omit them.
TCP success, RFB greetings and identical public-key fingerprints do not prove
complete authentication. A probe can trigger an incomplete-authentication warning.

### Protected Companion report transfer

1. Receiver: Analyze PC, **Receive a report**, select network interface and
   temporary port (TCP 11110 or 0 for automatic), then **Generate invitation and wait**.
2. Transfer the private invitation with **Copy invitation** or **Save invitation**.
   It is not a Veyon key and must not be imported into Configurator.
3. Other PC: Analyze PC, **Send my report**, paste/open the invitation,
   **Verify destination and send my report**, then confirm the data shared.
4. Wait for **DELIVERED** and **RECEIVED**. Reverse direction requires a new
   invitation. **Cancel connection** stops waiting.

The channel uses TLS 1.2+, a temporary certificate, SHA-256 pin and one-use
256-bit secret. Compact NL2 retains complete secrets; NL1 invitations are accepted,
but older clients cannot read NL2. Invitations expire within five minutes
and allow one report. No automatic listener, firewall modification or remote command
is added. Any firewall authorization must be local and narrowly scoped to the receiver.
Reports are unsigned snapshots, not trusted system attestations.
