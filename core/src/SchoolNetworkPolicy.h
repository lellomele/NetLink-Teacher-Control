// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QJsonObject>
#include <QLocalServer>
#include <QTimer>
#include <QElapsedTimer>
#include <QVector>
#include <QUuid>
#include <functional>
#include "VeyonCore.h"

class PolicyReliabilityTests;
namespace SchoolNetwork {
constexpr const char* PipeName = "VeyonSchool.NetworkPolicy.v1";
VEYON_CORE_EXPORT QJsonObject request(const QJsonObject& command);
VEYON_CORE_EXPORT bool validAddress(const QString& address);
VEYON_CORE_EXPORT bool configureDiscoveryFirewall(bool enabled);
VEYON_CORE_EXPORT QJsonObject nativeSelfTest();

class PolicyDaemon : public QObject {
public:
    explicit PolicyDaemon(QObject* parent);
    ~PolicyDaemon() override;
private:
    friend class ::PolicyReliabilityTests;
    using CloseEngine = std::function<quint32(void*)>;
    PolicyDaemon(QObject* parent, bool initializePipe, CloseEngine closeEngine);
    QJsonObject apply(const QJsonObject& command);
    quint32 release();
    void maintainLease();
    quint32 verifyFilters() const;
    QLocalServer m_server;
    QTimer m_timer;
    QElapsedTimer m_lease;
    QString m_owner;
    QByteArray m_digest;
    void* m_engine = nullptr;
    QVector<quint64> m_filters;
    quint32 m_cleanupError = 0;
    CloseEngine m_closeEngine;
    QElapsedTimer m_cleanupRetry;
    int m_cleanupAttempts = 0;
    int m_pipeClients = 0;
};
}
