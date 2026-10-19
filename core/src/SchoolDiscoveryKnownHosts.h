// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#pragma once

#include <QHash>
#include <QHostInfo>
#include <QQueue>
#include <QSet>
#include <QTimer>
#include <functional>
#include "VeyonCore.h"

// A dialog-local index of configured destinations, never of names advertised by
// unauthenticated discovery peers. DNS work is asynchronous, bounded and owned.
class VEYON_CORE_EXPORT SchoolDiscoveryKnownHosts : public QObject {
    Q_OBJECT
public:
    explicit SchoolDiscoveryKnownHosts(const QStringList& hosts, QObject* parent = nullptr);
    ~SchoolDiscoveryKnownHosts() override;
    void start();
    bool contains(const QString& address) const;
    void rememberAddress(const QString& address);
    int pendingCount() const { return m_pending.size() + m_lookups.size(); }
    int unresolvedCount() const { return m_unresolved; }
Q_SIGNALS:
    void changed();
private:
    friend class SchoolLifecycleTests;
    using Lookup = std::function<int(const QString&, QObject*, const std::function<void(const QHostInfo&)>&)>;
    using Abort = std::function<void(int)>;
    SchoolDiscoveryKnownHosts(const QStringList&, QObject*, Lookup, Abort, int timeout);
    void pump();
    void resolved(const QHostInfo& info);
    void cancel();
    QSet<quint32> m_addresses;
    QQueue<QString> m_pending;
    QHash<int, QString> m_lookups;
    QTimer m_deadline;
    Lookup m_lookup;
    Abort m_abort;
    int m_unresolved = 0;
    bool m_started = false, m_finished = false;
};
