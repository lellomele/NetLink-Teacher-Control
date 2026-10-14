// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QJsonObject>
#include <QLocalServer>
#include <QTimer>
#include <QElapsedTimer>
#include <QVector>
#include <QUuid>
#include "VeyonCore.h"

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
    QJsonObject apply(const QJsonObject& command);
    quint32 release();
    quint32 verifyFilters() const;
    QLocalServer m_server;
    QTimer m_timer;
    QElapsedTimer m_lease;
    QString m_owner;
    QByteArray m_digest;
    void* m_engine = nullptr;
    QVector<quint64> m_filters;
    quint32 m_cleanupError = 0;
};
}
