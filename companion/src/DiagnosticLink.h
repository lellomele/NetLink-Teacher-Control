// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025-26 prof. Ing. Raffaele Mele
#pragma once
#include <QObject>
#include <QHostAddress>
#include <QJsonObject>
#include <memory>

namespace Companion {
class DiagnosticLink : public QObject {
    Q_OBJECT
public:
    struct Endpoint { QHostAddress address; quint16 port=0; QByteArray pin; QByteArray token; };
    explicit DiagnosticLink(QObject* parent=nullptr);
    ~DiagnosticLink() override;
    bool receiveOnce(const QHostAddress& address, quint16 port=0, int lifetimeMs=300000);
    bool sendReport(const QString& code, const QJsonObject& report);
    void stop();
    QString pairingCode() const;
    static bool decodeCode(const QString& code, Endpoint& endpoint);
Q_SIGNALS:
    void reportReceived(const QJsonObject& report);
    void reportSent();
    void failed(const QString& message);
    void stopped();
private:
    struct Impl;
    std::unique_ptr<Impl> d;
};
}
