# Audit and release gates — NetLink 0.1.0-alpha.12

Status: **partial audit and preliminary implementation, not a completed security
audit, certified WS-Discovery implementation or production-ready release**.

## Alpha 12 installer recovery and removal

Recovery no longer starts an installed Qt CLI before replacing its missing
runtime. A statically linked Windows-only maintenance helper exports the machine
configuration registry (both views) and preserves previous backups in an
administrators/SYSTEM-only ProgramData recovery directory. The installer verifies
the payload's SHA-256 list before starting the new CLI. Stop/delete waits are
bounded and validate the exact fork service executable path; another service
location is never stopped automatically. A mutex prevents concurrent installer
and uninstaller instances. Pending removals under the fixed installation path
block reinstallation until reboot, avoiding deletion of freshly installed files.

Uninstall offers configuration retention or explicit purge. The fixed application
tree is removed, including unknown/manual files; junction targets are never
traversed. In-use files and their directories are scheduled for reboot. Custom
authentication paths inside that tree, unresolved paths or redirected key paths
stop removal first, rather than risking key loss. External key files are never
opened/copied/deleted by the helper. Purge retains only configured key-location
values in product settings, protects detected original Veyon shared settings and
cleans fork appearance settings in loaded profiles plus owned event-source
registrations. Unloaded profiles and OS-managed caches/history are not purged.
Firewall cleanup is restricted by the rule's actual application path.

The unit fixture uses exact HKCU-only test subtrees and private build directories;
the full production NSIS script is also exercised with a test-only helper/CLI.
Those fixtures deliberately simulate SCM and firewall operations and do not
establish privileged installation, in-use reboot cleanup or full rollback on a
real PC. Follow PILOT.md before treating this alpha as validated for a classroom.

## Alpha 11 focused changes

Legacy `UI/ApplicationName=Veyon` and the unchanged upstream default no longer
override NetLink branding. Custom school names remain supported; native window
titles end with the display name to avoid Qt adding it a second time. Toolbar
buttons use explicit natural-width minimums after font/style changes. Overflow
must expose complete operable buttons (Qt may expand into rows or use a menu).
Spectrum adds semantic colors using the existing vector engine, respecting
palette contrast and high-contrast monochrome fallback.

Configured language now sets QLocale even for English, loads regional catalogs
with language fallback, and cannot silently fall back to the Windows language
when an explicit selection is missing. Embedded English/Italian NetLink UI
strings use stable English source IDs; preset storage IDs are unchanged.
Apply and restart applications to change language. Native Windows dialogs can
still use the OS language; Companion diagnostic prose is not fully localized.

ScreenLock reuses the existing ordinary-key hook only while its worker is active.
The callback suppresses repeat events, bounds the queue and does not emit under
its mutex. Disabled trappers stop their timers; installation failure is reported.
The cover ignores ordinary close, restores on return to its desktop and does not
switch/take over Winlogon/UAC, change Windows policies or synthesize SAS.

New optional lock-event messages use the existing authenticated feature channel:
worker IPC nonce verification, current lock token, monotonic sequence, fixed
allowlist/count validation and rate limit precede relay to the initiating Master.
Only the most recent initiating Master receives events. Remote peers cannot
inject worker events. Existing Lock/Unlock command values and feature UIDs are
unchanged; legacy Veyon clients simply do not produce these optional messages.
There is no new listener, runtime library, driver or persisted input log.

During lock only, WTS reports session lock/unlock and a 500 ms read-only input
desktop check distinguishes Winlogon, another desktop and query failure. These
are observations, not proof of the exact triggering shortcut: Ctrl+Alt+Del,
Win+L, UAC, system actions and other transitions can overlap. Very short desktop
transitions can be missed. Ordinary reported shortcuts contain no typed text;
the Windows-key event does not infer a Win+letter combination. Counts aggregate
in two-second batches; timestamps shown are receipt times. The nonmodal Master
panel retains at most 200 plain-text lines in memory; no offline delivery,
durable audit trail or tamper-proof endpoint is claimed. Both PCs need updated
NetLink for event reporting. Native shortcut/session behavior remains subject
to the physical Windows two-PC pilot; isolated tests never install a real hook.

Local verification of this follow-up passed all 24 CTest suites: 243 Qt passes
(including setup/cleanup and three scale runs), no failures/skips, and all six
discovery XML schema checks. Lock-notification and GUI-scale suites also passed
five consecutive repetitions. The WITH_TESTS=OFF production build passed. The
published alpha.10 installer hash remains unchanged. Native desktop/session/hook behavior
has not yet been validated on the DELL/MATEBOOK pair.
The lifecycle locale fixtures exercise Italian and English explicitly, rather
than depending on the build host's default language. Headless CI uses Windows'
installed font directory, not an additional redistributable font requirement.

## Beta acceptance gates

Beta is a limited field-test milestone, not a claim of a completed security audit
or inviolable classroom controls. Freeze the feature scope and require:

- Guided Windows 10 x64 installation/upgrade/reboot on MATEBOOK and DELL with
  preserved configuration/keys, visible tray, working authentication and remote
  control; separately verify clean install, uninstall and recovery from failure
  on disposable systems. Automated installer fixtures do not replace this.
- Native lock/Unlock, ordinary shortcuts, Win+L, Ctrl+Alt+Del and UAC transitions,
  with correct and bounded event reporting, no false identification of a protected
  shortcut, no stuck input after Unlock and no full-desktop flicker/crash.
- InternetGuard allow/block/release/expiry under active connections, client/Master
  restart, Wi-Fi loss, IPv4/IPv6, and relevant firewall/antivirus configurations;
  verify actual traffic separately from successful WFP filter installation.
- Attendance/discovery regressions, language/DPI/overflow tests on native Windows,
  repeated classroom-like sessions and a representative multi-client load with
  recorded latency, CPU/memory and no unresolved critical/high-severity faults.
- Real routed-subnet/VLAN discovery and ACL tests before calling that scenario
  beta-supported. The current two Wi-Fi PCs on 192.168.1.x cannot establish it.
- Targeted security/dependency review of authentication, worker IPC, parsers,
  WFP authorization/leases, native input cleanup and diagnostic data handling;
  known limitations and reproducible release sources/hashes must be documented.

Code signing is recommended for distributing beyond controlled pilots, but a
signature is not evidence of correctness or a substitute for these tests. No
beta date is committed until the blocking native/resilience tests are complete.

## Alpha 10 focused GUI changes

Standard Qt tool buttons replace the custom parentless hover window and fixed
raster/font-ratio sizing. SVG/font dependencies are not added: original QPainter
icon paths are rendered at the requested resolution. Native focus, accessible
names, popup menus, checked state and toolbar overflow are retained. Tooltip
content is HTML-escaped; app/feature labels cannot inject markup.

Configurator view selection is stored separately per user. Easy filters only
known pages/controls; it does not recreate settings or overwrite hidden values.
Directory table population blocks edit signals, clears stale rows, preserves
group identity and does not rebuild the editor inside cellChanged. Destructive
group removal defaults to No. Add/remove commands are above both tables.

The tray snapshot uses local cached process/listener state without DNS, service
control or WFP pipe I/O on the event loop. Authorized client identities are
tracked by main-thread success notifications and intersected with live clients,
not by racing background authentication state from a periodic query. Native
tooltip length is bounded; its details dialog is plain text, single-instance,
non-quitting and self-deleting. Closing it must not terminate the tray worker.
Configured discovery is not represented as network reachability or WFP success.

The focused UI suites run at three offscreen scale factors using an isolated
configuration and inert classroom data. Real monitor changes, Windows tray
hover/Explorer recovery, privileged install and remote pointer alignment still
need the guided two-PC pilot before this alpha can be considered stable.

Copyright is a Unicode © suffix in native window titles, with no extra footer
in the Console, Configurator, Companion, School panels or service details. Native
Windows caption font/color, frame controls and accessibility are preserved: there
is no custom title bar, per-word font override or gradient. Existing legal/about
attributions and dedicated information menus are retained.

Local alpha.10 verification passed all 23 CTest suites: 214 Qt passes (including
initialization/cleanup and three independent scale cases), zero failures/skips,
plus the native NSIS upgrade fixture and six independent XML schema validations.
Each GUI scale suite also passed five consecutive until-fail repetitions.
The separate WITH_TESTS=OFF production build and 245-file payload validation
passed, including all eight PE identities/versions, SHA-256 hashes and forbidden
driver/test artifact checks. The original alpha.9 installer remains unchanged;
this alpha does not claim the pending privileged/native two-PC pilot is complete.

## Alpha 9 focused changes

Only the seven Windows executable output names change to `netlink-*`; Companion
already had its final name. DLLs, CMake target identities, protected install path,
registry/key locations, service/IPC identifiers and network feature/protocol IDs
remain unchanged. NetLinkIdentity supplies names for local server/worker/service
launch, discovery firewall rules, WFP application exceptions and exact full-path
pipe peer checks. The SYSTEM SID check is retained. Legacy filename recognition
is explicitly diagnostic-only and cannot authorize a WFP peer.

The installer selects the previous installed CLI before any replacement, exports
configuration, stops/unregisters the existing fork service with checked process
launch/exit status, then registers the current service image. A registered service
without a usable CLI or an incomplete file copy aborts the upgrade. Current Start
shortcuts and uninstall commands use the new names. Only the seven exact old EXE
paths in the fixed fork directory are retired after successful new service start;
locked old EXEs may require reboot. Keys, backups, DLLs, unknown files and any
separate original Veyon installation are not retirement targets.

A non-elevated NSIS fixture exercises the same installed-CLI selection and file
retirement macros for fresh/alpha.8/alpha.9/mixed directory states, using inert
files and no production service/registry/reboot operations. It checks preservation
of keys, backups, current binaries and unknown files. C++ tests cover the local
launch paths, strict current installed-path matching, legacy alias rejection and
diagnostic handling of both naming generations. Companion event/process parsing
retains old report support while recognizing all renamed applications.

This does not establish a completed privileged installer upgrade or mixed-client
interoperability test. An actual alpha.8→alpha.9 upgrade, reinstallation, reboot,
uninstall and original Veyon 4.7.5 client test remain guided pilot checks. The user
reported that the alpha.8 tray icon reappeared on MATEBOOK; the renamed worker path
must still be checked after alpha.9 installation. No changes to the retained
protocol/authentication implementation are needed for executable renaming.

Local alpha.9 verification passed 20 CTest suites, including 178 Qt passes
(setup/cleanup included), no failures or skips, and the native NSIS migration
fixture. The six independent XML schema checks passed. A separate production
build completed with all eight current EXE identities. These results do not
replace the privileged installation and original-client interoperability pilot.

The remote Windows Server pressure test exposed a fixture assumption: after four
non-writable polls for small sends, the OS still accepted the entire 32 MiB raw
write in 688 ms. A successful write before the deadline is not a production
timeout failure, but it did not exercise a stalled write. The fixture now limits
the listening socket's receive buffer before connecting, requests no additional
Winsock send buffering and logs/checks the effective receive limit. The actual
raw/TLS deadline assertions remain mandatory; production socket code is unchanged.
CI always prints Qt result files on failure and repeats the pressure suite twenty
times without retry-until-pass behavior. These isolated loopback tests do not
replace a slow/unreachable physical-client pilot.

## Alpha 8 focused changes

The discovery list accepts native and mapped IPv4 only and excludes the local
computer by observed sender addresses assigned to its interfaces, including
virtual/down adapters and loopback. Hostnames, cloned machine endpoints and
private subnets are not used as exclusion criteria. Local observations are not
cached; refreshing the cached interface inventory also removes observations
that have become local. The interface inventory is not queried for every row
or two-second UI refresh. A disappearing row clears the current selection rather
than leaving a different computer selected for import. IPv6 Windows/network
policy support is not disabled. IPv6 announcements cannot consume IPv4 message
IDs; Probe/Resolve requests are answered once per address family. A client now
answers Resolve for its own client endpoint instead of treating it as a proxy.

Windows user workers obtain a primary token for the exact requested SID and
actual server session, preferring WTS and then a verified interactive shell
before other candidates. Unusable/protected processes are skipped, process
access rights are limited, and opened tokens are revalidated against PID reuse.
Token/environment/profile/process-thread handles have scoped cleanup and any
previous thread impersonation is restored after launch. Startup logging omits
command-line values. Token tests inspect/copy metadata and check identity,
session, restricted-token rejection and repeated handle cleanup; they do not
launch a desktop process or prove tray rendering under SYSTEM after login.

Local history comparison found the first-user-process/full-access fragility
already in upstream 4.7.5 and the initial School alpha; no direct branding change
was established as the cause of the MATEBOOK symptom. An absent worker snapshot
does not distinguish failed creation from early exit. The installed-PC tray
check remains a release pilot requirement, not a confirmed fix result.

Focused discovery tests include Wi-Fi/virtual/mapped/loopback self exclusion,
remote peers with the same name or endpoint, routed private IPv4 addresses,
IPv6-before-IPv4 duplicate ordering, client/proxy Resolve and safe UI selection.
The real HTTP handler is tested on an isolated ephemeral loopback port, not the
production discovery port; this no longer skips when the installed service is
running. These fixtures do not establish multicast or cross-subnet reachability.

The alpha.8 application and separate production build passed locally. All 18
CTest suites passed, including 171 Qt passes (setup/cleanup included), no failures
and no skips. The six SOAP fixtures passed the independent schema validator.
The focused discovery/lifecycle and protocol suites also passed five consecutive
runs before the version bump. The additional native suites retain the hook and
region-queue stress coverage described below; no live system-wide hook, privileged
WFP policy, service restart or installation was performed by this test run.

## Alpha 7 focused changes

Attendance uses a non-quitting student dialog and an explicit one-shot worker
shutdown: Qt drains the queued TCP reply before disconnect, with a two-second
deadline instead of a blocking wait or arbitrary sleep. An isolated child-process
regression test exercises the real student dialog, FeatureMessage framing and
worker IPC transport with normal last-window behavior, both confirmation and
cancellation, and checks the resulting Master-side student name.

Worker launch success is no longer treated as indefinite evidence of liveness:
an unauthenticated startup expires after ten seconds, and stale deadlines cannot
remove a replacement or an authenticated worker. The tray feature retries a
missing worker every fifteen seconds using its cached tooltip, without DNS or
network polling. Startup failures log the feature identifier and queue count,
not the authentication nonce. Windows launch logs now omit argument/environment
values and worker reply logs omit student names. This addresses missed login/desktop readiness and
pre-IPC launch failure paths; the reported MATEBOOK tray symptom still requires
interactive confirmation after installing this version. Companion now inventories
suite process names/PIDs/sessions and the local worker IPC listener, without
collecting command lines or tokens, and distinguishes configured hiding/manual
service startup from a missing session worker.

Companion separates teacher-to-client control, client-to-teacher direct discovery
and custom TCP checks, supplies direction-specific guidance and saves at most
twenty session results in full reports. Reduced reports exclude these records
and the process inventory. A positive TCP check is not authentication, a complete
discovery exchange or UDP multicast verification.

The protected diagnostic link has separate receive/send views, role guidance,
ordered steps, cancellation, countdown and optional invitation files. NL2 packs
the address/port, complete SHA-256 certificate pin and independent 256-bit random
token into a bounded binary representation; IPv4 invitations are 104 characters
instead of approximately 188. The four-byte checksum detects copying mistakes;
it is not authentication, which remains TLS pinning plus the one-shot token.
No cryptographic material is shortened and no new runtime dependency is added.
IPv6 including bounded scope IDs and legacy NL1 decoding are retained. Expiry,
one-shot claiming, attempt limits and data-only permissions remain unchanged.
An invitation file is sensitive and does not extend its five-minute validity.

Focused tests cover the worker startup/replacement/disconnect/queue lifecycle,
the real attendance worker exit, probe directions and history privacy, UI role
switching, compact/legacy invitation round trips, corruption/truncation and TLS
pin/token rejection. They do not establish end-to-end classroom stability,
successful tray rendering after login, cross-subnet discovery or effective WFP
isolation. Installation and those checks remain guided two-PC pilot gates.

## Verified locally

Windows 10 Pro x64 10.0.19045, GCC 16.1.0 and Qt 5.15.18. Complete application
build and a separate production build without developer plugin paths/test hooks.
13 protocol/input Qt tests passed: address validation, every frame-fragment boundary, oversized
headers, invalid message types, namespace resolution, independent Probe fixture,
HTTP discovery on loopback, fixture export and 10,000 malformed XML inputs.
12 GUI configuration tests additionally exercise Apply, per-field and combined
edits, persistence/reopen with JSON and the real LocalStore/QSettings code on an
isolated INI backend, reset/discard and pre-save resource validation. The latter
tests use offscreen Qt widgets and never write live registry/configuration keys.
Eight appearance Qt passes (six scenarios plus setup/cleanup) cover isolated user
preference persistence/reopen, invalid input fallback, light/dark/system palettes,
black/white foreground contrast over 4,096 accent colors, preview cancellation and
dialog Save. Windows high-contrast switching, mixed-DPI monitors and full classroom
UI usability still require interactive verification. These are not a full
accessibility certification. NetLink preserves internal service, IPC, settings and
installation identities for compatibility; this is a public-facing rebrand.
Companion has dedicated fixture tests for report parsing/bounds, invalid targets,
identifier omission by whitelist projection, HTML escaping, static comparison,
elevated-token warnings, asymmetric masks, GUI rendering and a TCP probe against
an isolated loopback fixture only. No Veyon private-key file contents are read; no privileged
configuration writes are implemented. Native collection and permission results
have passed one read-only local Windows smoke check, retaining the report only in
memory and displaying counts rather than identifiers. Collection/permission results
still need validation under standard teacher, student and alternate-admin accounts,
domain policies, RDP, IPv6 and multiple adapters. Firewall rule inventory is partial,
not an effective-policy proof. Reports are unsigned snapshots, not trusted identity
or configuration. TCP reachability is not authentication. Collection runs in a
bounded child process for responsiveness, not a security sandbox or privilege boundary.
All six discovery message forms pass the independent .NET validator using the
official OASIS/W3C XML schemas. XML validity is not full protocol conformance.
Packaged CLI loading is checked with only Windows system directories in PATH.
The local non-elevated WFP self-test reached the engine but sublayer creation
returned Windows error 5 (access denied); privileged WFP execution is NOT verified.

Alpha.5 adds eight transport Qt passes, eight diagnostic-link Qt passes and
fifteen Companion Qt passes (including setup/cleanup), for six passing suites
and 64 Qt passes in total. The user-authorized two-PC test with a fresh key
completed authentication and received the first framebuffer update through the
corrected VeyonConnection implementation; no input was sent or image saved.
TLS tests cover correct pairing, wrong pins/tokens, expiry, replay rejection,
oversized pre-authentication frames and explicit stop. Widget destruction with
pending reception is covered after correcting destructor signal reentrancy.
A focused cppcheck run on DiagnosticLink could not parse the SDK's QT_CONFIG
macro; this is an analysis coverage gap, not a clean static-analysis result.

Alpha.6 passes all 16 local CTest suites: 139 Qt passes, no failures, and one
skipped discovery HTTP-loopback case because the active installed service already
owns port 3702. The six exported discovery messages pass the independent schema
validator. The 13 Qt suites include 17 connection, 18 School lifecycle, eight
authentication-wait, eight WFP cleanup, seven TCP/TLS-write, seven health and six
GUI-latency passes; the diagnostic-link suite now has 13 passes. Counts include
setup/cleanup. Three additional native suites cover 80,000 concurrent mock hook
callbacks, 200 inert DLL load/unload cycles and three 16,384-update region-queue
stress cycles. No global hooks or real privileged WFP filters are installed by
those tests. Five repeated warm School lifecycle runs passed, with a maximum
offscreen dialog opening of six milliseconds after console/font initialization;
this is not an interactive Windows/classroom latency guarantee.
Five repeated runs of the six native/connection/authentication/cleanup suites
and five diagnostic-link runs also passed. Extended TCP/TLS repetition exposed a
fixture assumption: one Windows `WSAEWOULDBLOCK` can be transient, and a later
large send may legitimately be accepted entirely by the kernel. The fixture now
allocates before prefill, validates the exact error and requires four consecutive
75 ms non-writable polls, with three-second/64 MiB preparation caps. Twenty
complete write-suite repetitions then passed (140 Qt passes); the production
write code was unchanged by this fixture correction.

The exact source-archive patch path was tested twice on fresh pinned vendor
trees nested inside this checkout without their own Git metadata. All 21 changed
vendor files match the working sources after line-ending normalization; hook and
native tests rebuilt successfully from those extracted trees.

## Corrections

- The desktop-crash investigation found Windows Application Error 1000 events
  for `dwm.exe`, naming the distributed winpthreads DLL. The installed import
  chain was `vnchooks → libstdc++ → libwinpthread`; the recorded offset maps to
  an abort after failed TLS-slot allocation. The replacement injected DLL uses
  only KERNEL32/USER32, no CRT/C++/pthread startup, TLS directory or delayed
  runtime imports. Loader attach/detach do no registry/User32 initialization;
  bounded callbacks do not allocate or wait for arbitrary window regions.
  Ownership publication, partial hook removal and logical input-filter
  deactivation are checked. The capture-region queue now uses a dedicated
  mutex with atomic union/drain, replacing a shared flag and polling loop.
  Concurrent producers/consumer verify the exact accumulated region and stable
  GDI resources after Windows cache warm-up. Mock hooks and inert DLL loading do not prove live
  desktop interoperability: the installed two-PC GUI test must confirm it.
- Network frames are decoded into worker-only storage and published as detached
  snapshots. Empty updates do not copy or repaint. Frame dimensions/bytes,
  outgoing queues and message/event batches are bounded. Stop/reconnect discard
  queued commands and obsolete callbacks; worker-owned client data is not
  modified from the GUI. Lost wakeups in stop and authentication predicate waits
  are corrected, with tests controlling the historical race window.
  Raw and OpenSSL writes share an absolute monotonic deadline, including
  non-reading peers and TLS WANT_READ/WANT_WRITE; partial progress cannot extend
  it indefinitely. This does not bound blocking DNS or every handshake operation.
- School windows are nonmodal/idempotent with client-selection snapshots and
  event-driven status updates. DNS resolution is cancellable and all-or-nothing;
  failed hosts cannot silently yield a partial policy. Attendance callbacks guard
  Worker lifetime and request IDs. WFP requests have concurrency limits and one
  pending Release per authenticated connection. Failed engine close preserves
  its handle, ownership and filters for backed-off retry, reports unknown, and
  prevents a new Apply from hiding pending cleanup. Active pipe clients are capped.
- Companion projects only relevant bounded Windows crash records and window
  response state; it never collects memory dumps or unrelated event messages.
  Deferred TLS notifications carry a session generation and cannot reach a newly
  paired session after cancellation. Callback-driven destruction is covered.
  A lightweight GUI timer records recovered event-loop gaps, not permanent
  deadlock proof; OS sleep/resume may also account for a gap.
- Source-archive vendor patching stops Git discovery at the parent of each vendor
  directory. This prevents an extracted tree nested inside another repository
  from silently reporting success while skipping its patches. Packaging rejects
  injected hooks with unexpected DLL imports, TLS or delayed import startup.
- Authentication framing distinguishes the exact-length blocking LibVNCClient
  SocketDevice from buffered/event-driven devices. The blocking path never peeks;
  asynchronous partial frames remain unconsumed. The adapter's Qt meta-object is
  exported across the Windows DLL boundary. Regression coverage includes real
  adapter construction outside the core DLL, auth types, empty acknowledgements,
  challenges, feature messages, oversized headers and truncated bodies.
- Companion highlights TCP outcomes without implying successful authentication,
  optionally reads an RFB greeting, and identifies disabled automatic firewall
  exceptions, missing listeners and loopback-only listeners. A public profile on
  any adapter is no longer presented as proof that the Wi-Fi profile is public.
- The optional Companion report link is one-shot TLS, not remote management.
  Certificates/keys and 256-bit pairing secrets are generated in memory; the
  sender pins SHA-256 even when no TLS verification error occurs. Pairing claims
  require a constant-time token comparison before report transfer, expire within
  five minutes, and close after one accepted session. Framing, report schema,
  connection count, attempts and phase deadlines are bounded. No firewall changes,
  key-file reads, shell commands or automatic listeners are added by this link.
  Local TLS fixture tests do not establish cross-subnet/firewall interoperability
  or replace independent review of the new network-facing code.
- School configuration fields update the staged model immediately, enabling Apply
  and persisting before post-save hooks. Reset blocks widget signals; connections
  are idempotent. Invalid/oversized resource lists are rejected by pre-save page
  validation, before any configuration/service/firewall change. GUI regression
  tests exercise each field, reopening a saved isolated configuration, clearing,
  reset/discard and rejection without partial persistence; no live HKLM or service
  writes are used by these tests.
- Complete-frame checks, initialized lengths, overflow-safe arithmetic, size
  limits, checked writes and feature UUID/command/map type validation.
- Single-use worker nonce, socket ownership, handshake timeout, bounded queues,
  unlocked I/O batches, bounded/cancellable session retries and parented kill timer.
  At most 16 pending local handshakes, with 16 KiB unauthenticated frames; larger
  buffers are enabled only after worker authentication.
- Monitoring queries no longer capture a destructible object or overlap; pending
  user/DNS queries are joined before teardown.
- Corrected the inherited test-build macro so an OFF production build cannot load
  developer plugin paths; plugin registration also rejects duplicate UUIDs.
- Worker read buffers can hold a complete permitted frame; socket rejection is
  outside worker-map locking. Service states are initialized and service start/stop
  waits are bounded to 30 seconds instead of waiting indefinitely.
- Authentication/access-control state checked explicitly before feature dispatch.
- UltraVNC initialized capture/IPC/compression/stream state, allocation checks,
  correct event-handle checks/reset, and non-throwing handle cleanup.
- LibVNCClient frees its client allocation after WinSock initialization failure.
  Qt HTTP response default status is initialized. Vendor corrections are pinned,
  source-visible patches applied idempotently at configuration time.
- Escaped tooltips, plain-text bounded Appello names, CSV formula neutralization
  and distinct student/Windows-account fields.
- Transactional WFP updates, dynamic-session cleanup, 90-second lease, explicit
  errors, DNS/DHCP infrastructure detection, loopback and IPv4/IPv6 support.
  Commit, status and renewal check installed filter IDs instead of trusting cached
  state; a lost session is reported/reset before renewal. Live BFE recovery and
  the lookup cost at the maximum destination count still need verification.
  The pipe requires SYSTEM and the expected installed service/server image paths;
  worker messages cannot request privileged network changes.
- Bounded discovery XML/HTTP size, depth, nodes, connections, timeouts, duplicate
  cache and sender rate. Metadata URLs are never fetched; IPs come from senders.
- Protected Program Files destination and migration backups; checked installer
  errors, preserved configuration/keys, exact manifest-based uninstall file list.

## Coverage limitations and open security work

The alpha.6 pass targets the identified crash path and concrete lifecycle,
concurrency and timeout defects; it is not a claim that every latent defect in
the application or its dependencies has been found. Local synthetic stress and
snapshot timings do not establish classroom performance. Native BFE/RPC may still
block service shutdown; a warning is emitted after five seconds rather than
forcibly terminating a transaction. Network-share CSV commit remains synchronous.
Blocking DNS and inherited forced-thread termination on exceptional destruction
need a separate cancellation design. Legacy capture teardown still contains
`g_lockcode`/keyboard-helper coordination and some unbounded joins; replacing
that protocol safely requires a separate lifecycle redesign, not forced unload
or a timeout that could leave callbacks using freed memory. GUI-gap warnings
appear only after recovery.

Cppcheck 2.19.1 scanned the compilation database, initially 336 compilation units,
including internal Windows libraries. Concrete issues above were corrected.
Some Qt/compiler configurations were not parsed: the second pass still had 43
syntax/preprocessing diagnostics and warnings needing triage. A zero tool exit
does NOT establish full analysis coverage or absence of bugs. External runtime
packages were inventoried, not independently audited in full.
The alpha.6 focused compilation-database scan of seven changed implementation
files also stopped in Qt/MinGW headers (`__has_builtin`, Qt deprecation macros).
Preprocessing GuiLatencyMonitor with the actual GCC flags succeeded, but the
analyzer still rejected the resulting translation unit. No application finding
was established by that scan; it adds no complete static-analysis coverage.

Sanitizers, sustained load/reentrancy testing, complete dependency CVE/backport
review and independent protocol interoperability remain release gates. The
inherited LibVNC/UltraVNC/KLdap/HTTP versions are not all current upstream.

Veyon 4.7.5 authentication is retained; general TLS/message-integrity protection
is not added to RFB in this alpha. Restrict authorization to teachers and do not
expose it to untrusted networks. The inherited embedded-key password encryption
is not a secret-storage boundary: persisted/logon credentials require a dedicated
OS-protected storage/protocol review. Appello does not transmit passwords.

Allowed WFP destinations remain reachable on every port; DNS is available to
configured resolvers. Proxies, VPNs, DNS tunnels, administrators and other firewall
providers need threat-model testing. ACK confirms committed filters, NOT actual
Internet unreachability. Existing flows, IPv6, QUIC, interface changes and 90-second
fail-open recovery require live tests.

Appello is self-declared; false/duplicate names are possible. Names are volatile
and do not survive Master restart/model recreation. Discovery is a bounded profile
for the Veyon client type, not a claim of arbitrary scope matching, signed discovery,
complete proxy suppression/fault semantics or unrelated WSD interoperability.

Opaque DeskDupEngine and kernel-driver binaries are omitted. Software capture and
input-lock performance/strength have NOT been shown equivalent to upstream's
accelerated/driver modes. Installer fresh install, update, rollback, reboot and
uninstall still require isolated-PC validation. This alpha is unsigned.

See PILOT.md for remaining evidence. A single-subnet Wi-Fi test cannot establish
cross-subnet discovery resilience or complete the security audit.

References: [WS-Discovery 1.1](https://docs.oasis-open.org/ws-dd/discovery/1.1/os/wsdd-discovery-1.1-spec-os.html),
[WFP arbitration](https://learn.microsoft.com/en-us/windows/win32/fwp/filter-arbitration).
