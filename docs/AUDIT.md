# Audit and release gates — 0.1.0-alpha.1

Status: **partial audit and preliminary implementation, not a completed security
audit, certified WS-Discovery implementation or production-ready release**.

## Verified locally

Windows 10 Pro x64 10.0.19045, GCC 16.1.0 and Qt 5.15.18. Complete application
build and a separate production build without developer plugin paths/test hooks.
13 Qt tests passed: address validation, every frame-fragment boundary, oversized
headers, invalid message types, namespace resolution, independent Probe fixture,
HTTP discovery on loopback, fixture export and 10,000 malformed XML inputs.
All six discovery message forms pass the independent .NET validator using the
official OASIS/W3C XML schemas. XML validity is not full protocol conformance.
Packaged CLI loading is checked with only Windows system directories in PATH.
The local non-elevated WFP self-test reached the engine but sublayer creation
returned Windows error 5 (access denied); privileged WFP execution is NOT verified.

## Corrections

- Complete-frame checks, initialized lengths, overflow-safe arithmetic, size
  limits, checked writes and feature UUID/command/map type validation.
- Single-use worker nonce, socket ownership, handshake timeout, bounded queues,
  unlocked I/O batches, bounded/cancellable session retries and parented kill timer.
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
  The pipe requires SYSTEM and the expected installed service/server image paths;
  worker messages cannot request privileged network changes.
- Bounded discovery XML/HTTP size, depth, nodes, connections, timeouts, duplicate
  cache and sender rate. Metadata URLs are never fetched; IPs come from senders.
- Protected Program Files destination and migration backups; checked installer
  errors, preserved configuration/keys, exact manifest-based uninstall file list.

## Coverage limitations and open security work

Cppcheck 2.19.1 scanned the compilation database, initially 336 compilation units,
including internal Windows libraries. Concrete issues above were corrected.
Some Qt/compiler configurations were not parsed: the second pass still had 43
syntax/preprocessing diagnostics and warnings needing triage. A zero tool exit
does NOT establish full analysis coverage or absence of bugs. External runtime
packages were inventoried, not independently audited in full.

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
