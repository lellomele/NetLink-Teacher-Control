// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#include "SchoolDiscoveryKnownHosts.h"
#include "HostAddress.h"
#include <QHostAddress>
#include <QUrl>

SchoolDiscoveryKnownHosts::SchoolDiscoveryKnownHosts(const QStringList& hosts, QObject* parent)
    : SchoolDiscoveryKnownHosts(hosts, parent,
        [](const QString& host, QObject* context, const std::function<void(const QHostInfo&)>& callback) {
            return QHostInfo::lookupHost(host, context, callback);
        }, [](int id) { QHostInfo::abortHostLookup(id); }, 5000) {}

SchoolDiscoveryKnownHosts::SchoolDiscoveryKnownHosts(const QStringList& hosts, QObject* parent,
    Lookup lookup, Abort abort, int timeout)
    : QObject(parent), m_deadline(this), m_lookup(std::move(lookup)), m_abort(std::move(abort)) {
    QSet<QString> unique;
    for(const auto& input : hosts) {
        auto host = input.trimmed().toLower();
        // Veyon's configured destinations may include an explicit TCP port.
        if(host.startsWith(QLatin1Char('[')) || host.count(QLatin1Char(':')) == 1) {
            const auto parsed=HostAddress::parseHost(host);
            if(!parsed.isEmpty()) host=parsed;
        }
        QHostAddress address;
        if(address.setAddress(host)) {
            rememberAddress(host);
            continue;
        }
        if(host.endsWith(QLatin1Char('.'))) host.chop(1);
        if(host.isEmpty() || unique.contains(host)) continue;
        unique.insert(host);
        // Only configured destinations reach DNS, never the peer's claimed name.
        if(host.size() > 253 || QUrl::toAce(host).isEmpty()) { ++m_unresolved; continue; }
        m_pending.enqueue(host);
    }
    m_deadline.setSingleShot(true);
    m_deadline.setInterval(timeout);
    connect(&m_deadline, &QTimer::timeout, this, [this] {
        m_unresolved += pendingCount();
        cancel();
        Q_EMIT changed(); // Failed/unresolved destinations leave peers visible.
    });
}

SchoolDiscoveryKnownHosts::~SchoolDiscoveryKnownHosts() { cancel(); }

bool SchoolDiscoveryKnownHosts::contains(const QString& address) const {
    const auto key=SchoolDiscovery::addressKey(QHostAddress(address));
    return !key.isEmpty() && m_addresses.contains(key);
}

bool SchoolDiscoveryKnownHosts::contains(const SchoolDiscovery::Record& record) const {
    if(record.conflict) return false; // Do not hide conflicting unauthenticated identities.
    if(contains(record.address)) return true;
    for(const auto& address:record.addresses) if(contains(address)) return true;
    return false;
}

void SchoolDiscoveryKnownHosts::rememberAddress(const QString& address) {
    const auto value=SchoolDiscovery::addressKey(QHostAddress(address));
    if(!value.isEmpty() && !m_addresses.contains(value)) {
        m_addresses.insert(value);
        Q_EMIT changed();
    }
}

void SchoolDiscoveryKnownHosts::start() {
    if(m_started || m_finished) return;
    m_started = true;
    m_deadline.start();
    pump();
    Q_EMIT changed();
}

void SchoolDiscoveryKnownHosts::pump() {
    while(!m_finished && m_lookups.size() < 8 && !m_pending.isEmpty()) {
        const auto host = m_pending.dequeue();
        const int id = m_lookup(host, this, [this](const QHostInfo& info) { resolved(info); });
        if(id < 0 || m_lookups.contains(id)) ++m_unresolved;
        else m_lookups.insert(id, host);
    }
    if(pendingCount() == 0) { m_finished = true; m_deadline.stop(); }
}

void SchoolDiscoveryKnownHosts::resolved(const QHostInfo& info) {
    if(m_finished || !m_lookups.remove(info.lookupId())) return;
    bool usable = false;
    if(info.error() == QHostInfo::NoError) for(const auto& address : info.addresses()) {
        const auto value=SchoolDiscovery::addressKey(address);
        if(!value.isEmpty()) { m_addresses.insert(value); usable = true; }
    }
    if(!usable) ++m_unresolved;
    pump();
    Q_EMIT changed();
}

void SchoolDiscoveryKnownHosts::cancel() {
    m_finished = true;
    m_deadline.stop();
    m_pending.clear();
    const auto ids = m_lookups.keys();
    m_lookups.clear();
    for(const auto id : ids) m_abort(id);
}
