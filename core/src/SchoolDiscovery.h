// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QDateTime>
#include <QHash>
#include <QNetworkAccessManager>
#include <QTcpServer>
#include <QTimer>
#include <QUdpSocket>
#include <QUuid>
#include "VeyonCore.h"

namespace SchoolDiscovery {
struct Record {
    QString endpoint, name, address;
    int port = 11100;
    qint64 seen = 0;
    quint64 instance = 0, sequence = 0;
    bool client = false;
    bool proxy = false;
};
struct Message {
    QString action, id, relatesTo, to, types, scopes, matchBy;
    QList<Record> records;
    QString resolve;
    quint64 instance = 0, sequence = 0;
};
VEYON_CORE_EXPORT bool decode(const QByteArray& xml, Message& output);
VEYON_CORE_EXPORT QByteArray encode(const Message& message);
class VEYON_CORE_EXPORT Agent : public QObject {
    Q_OBJECT
public:
    Agent(bool master, const QString& bootstrap, int port, QObject* parent);
    ~Agent() override;
    QList<Record> records() const;
    QString error() const { return m_error; }
    void probe();
private:
    Message announcement(const QString& action) const;
    void announce();
    void multicast(const QByteArray& data);
    void readUdp(QUdpSocket* socket);
    QByteArray process(const QByteArray& xml, const QHostAddress& source, bool http);
    void acceptHttp();
    bool m_master;
    QString m_bootstrap, m_error, m_endpoint;
    int m_port;
    QUdpSocket m_udp4, m_udp6;
    QUdpSocket m_probe4, m_probe6;
    QHash<QString,qint64> m_queries;
    QTcpServer m_http;
    QNetworkAccessManager m_network;
    QTimer m_timer;
    QHash<QString, Record> m_records;
    QHash<QString, qint64> m_seen;
    QHash<QString, int> m_rate;
    quint64 m_instance, m_sequence = 0;
    int m_httpClients = 0;
};
}
