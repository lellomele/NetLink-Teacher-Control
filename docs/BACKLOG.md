# Consolidation and future work

[English](BACKLOG.md) · [Italiano](BACKLOG.it.md)

Available features are described in the [README](../README.md)
and [user guide](USAGE.md). This list covers work still to assess or complete,
not a version history.

## Consolidation priorities

- Prolonged multi-client testing with latency, CPU and memory measurements.
- WFP recovery with existing connections, network loss, restarts, IPv6/QUIC,
  VPNs and the deployment's firewall/antivirus policies.
- Review inherited forced-thread-stop paths, blocking DNS and long native waits.
- Update dependency verification and close analysis-coverage gaps.
- Dedicated-PC tests for install, update, file locks, restart and recovery.
- Physical validation of mixed DPI, accessibility, input and the service tray icon.

## Scenarios requiring separate validation

- Discovery across subnets/VLANs and actual ACLs.
- IPv6-only networks and changes to link-local interface indices.
- Common-feature interoperability with original Veyon clients.
- Companion collection under standard/alternate-admin accounts, domains/RDP,
  multiple adapters and school policies.

## Features not included

The Companion channel transfers diagnostic reports through explicit pairing.
Assisted repairs need a specific proposal, local consent, least privilege,
outcome verification and rollback. Do not add remote commands or permanent
listeners to the diagnostic channel without that design.

New input drivers, capture acceleration or protocol extensions require
separate evaluation; they are not prerequisites for the current installer.

[Test checklist](PILOT.md) · [Safeguards and beta criteria](AUDIT.md)
