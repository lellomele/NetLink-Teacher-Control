# Features and configuration

[English](USAGE.md) · [Italiano](USAGE.it.md)

## Teacher Console and remote control

The Console reuses Configurator workstations. A real connection and updating
thumbnail establish authenticated exchange; a successful TCP test is insufficient.
New features require compatible NetLink clients.

The compact remote menu leaves the desktop visible. Each session starts on the
top edge. Its pin stays visible and fades after inactivity; hover restores it.
Left-click pins/unpins the toolbar. Right-click on the pin or toolbar rotates
top → left → bottom → right. Only one edge is active.
Menus and keyboard focus keep controls accessible.

Appearance, colors, sizes and icon sets are personal preferences. Remote controls
follow the Console's set. High contrast disables pin fading.
Choose language in Configurator, apply and restart applications.
Windows dialogs may still use the system language.

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

1. Select clients, open InternetGuard and choose **Block Internet**.
2. Read each client's confirmation, error or missing response.
3. Use **Unblock** to release filters. Closing the panel does not unblock.
4. **Check status** queries the service; it does not test browsing.

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

Limits: 256 clients per operation, 255 destinations and 128 extra resources.
Do not use scoped link-local IPv6 (`%number`) as InternetGuard destinations;
prefer routable IPs or resolvable hostnames.

## Student names and class–room profiles

**Attendance → Request names** opens the dialog on selected clients.
Students submit with OK or Enter. Captions show
`[attendance name] - Windows account - PC`, highlighting only the name.
There is no attendance-confirmation mechanism or verified identity.

In **Locations & computers → Class–room profiles…**:

1. Assign class, room and student to workstations. The editor starts with selected
   PCs, or displayed PCs when none are selected.
2. **Save JSON** preserves assignments; **Load JSON** restores them.
   This is not Configurator JSON, and profiles do not load automatically at startup.
3. Edit an assignment or **Use declared names** on selected rows.
   Replies never rewrite the file automatically: save explicitly.
4. **Export CSV** creates a readable copy of assignments and declarations,
   not a reloadable profile.
5. **End requests and clear declared names** keeps the profile;
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
Metadata addresses alone are not evidence. The teacher is excluded on every adapter.
Contradictory announcements stay visible and cannot be imported automatically.
Cloned PCs with identical identity/metadata may be indistinguishable.

**Hide computers already in Locations & computers** matches observed addresses
against configured IPs and DNS A/AAAA answers across all locations.
Unresolved names leave results visible; an old DHCP IP is not stable identity.
Disable the filter to see known PCs too.

Verify the destination before import. Configuration write rights and Master
restart are required to display a new PC. Import does not alter keys/access rules.
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
Also check Windows hidden icons. “Listening” does not establish reachability,
authentication, discovery or successful Internet blocking.

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
