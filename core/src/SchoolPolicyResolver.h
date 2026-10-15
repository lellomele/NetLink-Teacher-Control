// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QHash>
#include <QHostInfo>
#include <QTimer>
#include <functional>
#include "VeyonCore.h"

// One cancellable, all-or-nothing resolution batch. No worker or blocking DNS
// call is needed on the GUI thread, and late callbacks cannot complete a batch.
class VEYON_CORE_EXPORT SchoolPolicyResolver : public QObject {
    Q_OBJECT
public:
    SchoolPolicyResolver(const QStringList& hosts, const QStringList& resources, QObject* parent);
    ~SchoolPolicyResolver() override;
    void start();
    void cancel();
Q_SIGNALS:
    void finished(bool complete, const QStringList& addresses);
private:
    friend class SchoolLifecycleTests;
    using Lookup = std::function<int(const QString&, QObject*, const std::function<void(const QHostInfo&)>&)>;
    using Abort = std::function<void(int)>;
    SchoolPolicyResolver(const QStringList&, const QStringList&, QObject*, Lookup, Abort, int timeout);
    void resolved(const QHostInfo& info);
    void finish(bool complete);
    QStringList m_hosts, m_addresses;
    QHash<int, QString> m_outstanding;
    QTimer m_deadline;
    Lookup m_lookup;
    Abort m_abort;
    bool m_started = false, m_finished = false, m_failed = false, m_initializing = false;
};
