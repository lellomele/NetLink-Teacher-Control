# Architecture, safeguards and verification status

[English](AUDIT.md) · [Italiano](AUDIT.it.md)

The NetLink audit is **partial**. This document describes implemented mechanisms,
limitations and required tests. It does not certify freedom from vulnerabilities,
complete WS-Discovery compliance or production readiness.

## Communication and lifecycle

- Authentication and access control precede feature execution. The RFB adapter
  distinguishes exact blocking reads from partial asynchronous frames.
- Frames, image dimensions, queues, commands and event batches are bounded.
  The decoder publishes thumbnail snapshots separately from the worker buffer;
  empty updates avoid unnecessary copies and redraws.
- Stop/reconnect discard stale commands and callbacks. TCP/OpenSSL writes use
  absolute monotonic deadlines even when the peer stops reading or TLS retries.
- Local workers use single-use nonces, socket ownership checks, startup/authentication
  deadlines and bounded queues. Recovery timers follow object lifetime;
  unauthenticated messages have stricter limits.
- InternetGuard/Roll Call/discovery windows are non-modal, reusable and retain
  the set of clients selected when opened.
- DNS in new features is asynchronous and cancellable. A policy is not applied
  using only the resolved part of an incomplete list.

**Open issues:** inherited code still includes blocking DNS, exceptional forced
VNC thread termination and unbounded native waits on some shutdown paths.
These require lifecycle review, not timeouts that leave callbacks referencing
freed memory. GUI-delay warnings are emitted after recovery: they do not prove
a deadlock and may include Windows sleep/resume.

## InternetGuard and privileged service

WFP filters use a dynamic session, transactions and a 90-second lease.
Commit, status and renewal verify installed filter IDs.
The pipe verifies SYSTEM and expected service/server paths; GUI workers cannot
directly request privileged network changes.

Requests have concurrency limits; a connection can retain one explicit pending
unblock request. If WFP close fails, handles, ownership and recovery state are
retained for retry; a new block cannot hide incomplete cleanup.
The state is not reported as unrestricted without Windows confirmation.

Feedback confirms applied filters, not Internet unreachability.
Allowed destinations on all ports, DNS resolvers, proxies, VPNs, QUIC, existing
connections, adapter changes and other firewall providers require real tests.
Scoped link-local IPv6 addresses are not accepted as resources.
Blocked BFE/RPC calls can delay lease expiry or service shutdown.

Limits: 256 clients per operation, 255 destinations and 128 additional resources.

## Roll Call and profiles

Requests/responses have correlated IDs; stale replies, profile changes and
disconnects must not reuse old names. Captions keep the Roll Call name, Windows
account and PC separate. Strings are bounded and treated as text, not HTML.

Profiles exist only on the Master: versioned JSON schema, maximum 1 MiB and
256 assignments, valid/unique UUIDs and bounded strings. IP addresses and hostnames
are hints, not assignment identities. Invalid loads do not replace the active
profile. Preloaded names are distinct from declarations, not attendance confirmations.

JSON/CSV I/O runs outside the GUI thread. QSaveFile disallows direct-write fallback;
CSV formula prefixes are neutralized. On Windows, Qt permissions do not replace
NTFS ACLs: protect the folder. An already-started network-file write can complete
after the panel closes. Files contain plaintext personal data, not verified identities.

## IPv4/IPv6 discovery

Automatic mode prefers recently observed IPv4, then routable IPv6 and link-local.
IPv4 Only remains available. This selects announcements; it is not a TCP reachability
test or connection failover.

The cache bounds total observations and observations per family/identity, with
95-second expiry, sequences and per-source duplicate detection. A Bye removes
only its source observation. Actual sender addresses are used, not arbitrary
metadata aliases; no remote URL is followed.

Announcements sharing an EPR produce one row. Conflicting names/ports remain
visible and cannot be imported automatically. Cloned images with identical
identity/data cannot be distinguished without authentication.
The teacher is excluded using addresses on all adapters, not its name.

Link-local addresses use the receiving interface index, never the remote index.
Known-PC filtering compares all observed addresses with configured destinations
and DNS A/AAAA. At most eight DNS lookups run concurrently, with a five-second
deadline; errors keep results visible. Import revalidates mode, freshness,
conflicts and duplicates.

XML/HTTP limits cover size, depth, nodes, connections, rate and duration.
SOAP responses are limited to 32 KiB and 64 records; the cache holds at most
1,024 observations. XML schema validation does not certify the whole WS-Discovery
protocol. IPv6 multicast is link-local; routing, ACLs and cross-subnet scenarios
need separate testing.

## Interface, input and capture

The remote toolbar is a compact widget, not a full-desktop overlay.
Timers/animations belong to the widget; right-click changes the edge locally
without forwarding commands to the client. GUI tests cover focus, menus, DPI
and high contrast; real-monitor validation is still required.

Input lock uses the software mechanism for ordinary shortcuts. Session events
and protected-desktop observations are grouped, bounded and forwarded only to
the relevant Master. They do not collect typed text or identify Ctrl+Alt+Delete
with certainty. There is no forensic log or replay of events lost while disconnected.

The shipped hook DLL imports only KERNEL32/USER32, without CRT/pthread startup,
TLS or delayed dependencies. Callbacks and the region queue have dedicated limits
and synchronization. Interception and the binary DeskDupEngine are not shipped.
Software capture, input and inherited shutdown paths need load tests;
equivalence to accelerated/driver modes is not claimed.

## Companion and report protection

Collection runs in a time/size-bounded child process: this improves responsiveness,
not security sandboxing. Diagnostics distinguish listeners, network profile,
configured rules and TCP tests; they do not certify effective firewall policy
or authentication. Full reports contain identifiers; reduced reports exclude them.

The temporary connection uses TLS, a verified SHA-256 fingerprint and a single-use
256-bit secret compared in constant time. Frames, attempts, connections and the
single delivery are bounded and have deadlines. Notifications are tied to the
session generation, preventing delivery into a later pairing.
It starts no automatic listener, reads no Veyon private key and changes neither
firewall nor configuration. Received reports remain unsigned statements.

Remote control inherits the Veyon 4.7.5 security model: Companion TLS does not
provide general TLS/integrity protection for RFB. Persistent credentials need
a separate storage review; do not assume inherited embedded-key encryption
protects them from someone with system access. Do not expose the suite to untrusted networks.

## Installer and distribution

A Qt-free helper saves settings/backups, checks paths and replaceable files,
manages the fork service and verifies SHA-256 before starting newly installed
tools. It rejects redirected paths and does not modify external key files.
Concurrent operations are excluded.

Busy files require a restart without intentionally starting a partial copy;
permission errors are distinguished and files cannot be skipped.
This is not an atomic upgrade: the check cannot prevent later opens or provide
complete rollback.

Uninstall removes the whole application tree without following junctions,
protects key paths and offers settings preservation or cleanup.
Detected shared settings are retained; OS data and unloaded user profiles are
not deleted. Details/recovery: [INSTALLATION.md](INSTALLATION.md).

## Coverage and beta criteria

Tests cover framing, TCP/TLS deadlines, concurrency, teardown, authentication,
simulated WFP/input, callbacks, Roll Call/profiles, DNS/discovery, Companion and
multi-scale GUI. Six discovery message types are validated against official
schemas. The IPv6 UDP test uses real loopback; it explicitly skips without IPv6.

Installer tests use isolated files/registry subtrees and simulate service/firewall.
Distribution builds exclude test hooks/developer plugin paths.
Import checks, manifests and dependency source inventories are not full library audits.

Static analysis has Qt/MinGW preprocessing gaps and does not provide complete
coverage. Dependency/CVE review, backports, applicable sanitizers, prolonged
load/reentrancy and interoperability tests remain necessary. Not all inherited
libraries match the latest upstream versions.

For a controlled beta: freeze features, close critical/high defects, verify real
installation/recovery, native lock/unlock, WFP resilience, prolonged multi-client
operation, language/DPI and data protection. Support additional network scenarios
only after testing. Digital signing is separate from correctness and does not
replace these tests.

[Operational checklist](PILOT.md) · [Dependencies](DEPENDENCIES.md) ·
[WS-Discovery 1.1](https://docs.oasis-open.org/ws-dd/discovery/1.1/os/wsdd-discovery-1.1-spec-os.html) ·
[WFP arbitration](https://learn.microsoft.com/en-us/windows/win32/fwp/filter-arbitration)
