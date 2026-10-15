// SPDX-License-Identifier: GPL-2.0-or-later
#include "SchoolPolicyResolver.h"
#include "SchoolNetworkPolicy.h"
#include <QHostAddress>

namespace {
QString canonical(QHostAddress address) {
    bool ipv4 = false;
    const auto value = address.toIPv4Address(&ipv4);
    if(ipv4) return QHostAddress(value).toString();
    return address.toString();
}
}
SchoolPolicyResolver::SchoolPolicyResolver(const QStringList& hosts, const QStringList& resources, QObject* parent)
    : SchoolPolicyResolver(hosts, resources, parent,
        [](const QString& host, QObject* context, const std::function<void(const QHostInfo&)>& callback) {
            return QHostInfo::lookupHost(host, context, callback);
        }, [](int id) { QHostInfo::abortHostLookup(id); }, 5000) {}

SchoolPolicyResolver::SchoolPolicyResolver(const QStringList& hosts, const QStringList& resources,
    QObject* parent, Lookup lookup, Abort abort, int timeout)
    : QObject(parent), m_hosts(hosts), m_addresses(resources), m_deadline(this),
      m_lookup(std::move(lookup)), m_abort(std::move(abort)) {
    m_deadline.setSingleShot(true);
    m_deadline.setInterval(timeout);
    connect(&m_deadline, &QTimer::timeout, this, [this] {
        qWarning("NetLink policy DNS deadline expired; no policy was applied");
        finish(false);
    });
}
SchoolPolicyResolver::~SchoolPolicyResolver() { cancel(); }
void SchoolPolicyResolver::cancel() {
    if(m_finished) return;
    m_finished = true;
    m_deadline.stop();
    const auto ids = m_outstanding.keys();
    m_outstanding.clear();
    for(int id : ids) m_abort(id);
}
void SchoolPolicyResolver::start() {
    if(m_started || m_finished) return;
    m_started = true;
    m_initializing = true;
    m_hosts.removeDuplicates();
    if(m_hosts.isEmpty() || m_hosts.size() > 256 || m_addresses.size() > 255) m_failed = true;
    for(const auto& resource : m_addresses) if(!SchoolNetwork::validAddress(resource)) m_failed = true;
    if(!m_failed) for(const auto& host : m_hosts) {
        const QHostAddress address(host);
        if(!address.isNull()) {
            const auto value = canonical(address);
            if(SchoolNetwork::validAddress(value)) m_addresses.append(value);
            else m_failed = true;
        } else if(host.isEmpty() || host.size() > 253) m_failed = true;
        else {
            const int id = m_lookup(host, this, [this](const QHostInfo& info) { resolved(info); });
            if(id < 0 || m_outstanding.contains(id)) m_failed = true;
            else m_outstanding.insert(id, host);
        }
    }
    m_initializing = false;
    if(m_failed || m_outstanding.isEmpty()) finish(!m_failed);
    else m_deadline.start();
}
void SchoolPolicyResolver::resolved(const QHostInfo& info) {
    if(m_finished || !m_outstanding.remove(info.lookupId())) return;
    bool usable = false;
    if(info.error() == QHostInfo::NoError) for(const auto& address : info.addresses()) {
        const auto value = canonical(address);
        if(SchoolNetwork::validAddress(value)) { usable = true; m_addresses.append(value); }
    }
    if(!usable) m_failed = true;
    if(!m_initializing && (m_failed || m_outstanding.isEmpty())) finish(!m_failed);
}
void SchoolPolicyResolver::finish(bool complete) {
    if(m_finished) return;
    m_addresses.removeDuplicates();
    complete = complete && !m_failed && m_outstanding.isEmpty() && !m_addresses.isEmpty() && m_addresses.size() <= 255;
    cancel();
    Q_EMIT finished(complete, complete ? m_addresses : QStringList{});
}
