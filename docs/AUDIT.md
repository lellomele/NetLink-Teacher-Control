# Audit and release gates — NetLink 0.1.0-alpha.6

Status: **partial audit and preliminary implementation, not a completed security
audit, certified WS-Discovery implementation or production-ready release**.

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
