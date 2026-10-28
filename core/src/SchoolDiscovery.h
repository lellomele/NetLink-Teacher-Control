// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QDateTime>
#include <QElapsedTimer>
#include <QHash>
#include <QNetworkAccessManager>
#include <QNetworkInterface>
#include <QTcpServer>
#include <QTimer>
#include <QUdpSocket>
#include <QUuid>
#include <QSet>
#include "VeyonCore.h"

class SchoolLifecycleTests;
class SchoolTests;
namespace SchoolDiscovery {
enum class AddressMode { Automatic, Ipv4Only };
// Canonical unicast destination. Link-local IPv6 requires a local interface;
// the remote peer's advertised zone number is never used for reception.
VEYON_CORE_EXPORT QString addressKey(const QHostAddress& address, uint interfaceIndex = 0);
struct Record {
    struct Observation {
        QString address, name;
        int port = 11100;
        bool operator==(const Observation& other) const {return address==other.address && name==other.name && port==other.port;}
    };
    QString endpoint, name, address;
    QStringList addresses;
    int port = 11100;
    qint64 seen = 0;
    quint64 instance = 0, sequence = 0;
    bool client = false;
    bool proxy = false;
    bool conflict = false;
    // Local observations only: never serialized into discovery announcements.
    QList<Observation> observations;
};
VEYON_CORE_EXPORT QString conflictSummary(const Record&);
VEYON_CORE_EXPORT QString conflictDetails(const Record&);
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
    QList<Record> records(AddressMode mode = AddressMode::Automatic) const;
    bool isCurrent(const Record& selected, AddressMode mode) const;
    QString error() const { return m_error; }
    void probe();
private:
    friend class ::SchoolLifecycleTests;
    friend class ::SchoolTests;
    Agent(bool master, const QString& bootstrap, int port, QObject* parent, bool initializeSockets);
    Message announcement(const QString& action) const;
    void refreshInterfaces();
    bool isLocalIpv4(const QHostAddress& address) const;
    bool isLocalAddress(const QHostAddress& address) const;
    void announce();
    void multicast(const QByteArray& data);
    void readUdp(QUdpSocket* socket);
    QByteArray process(const QByteArray& xml, const QHostAddress& source, bool http, uint interfaceIndex = 0);
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
    QList<QNetworkInterface> m_interfaces;
    QSet<quint32> m_localIpv4;
    QSet<QString> m_localIpv6;
    QSet<uint> m_joined4, m_joined6;
    QSet<QString> m_membershipErrors;
    QSet<QUdpSocket*> m_udpDrainScheduled;
    QElapsedTimer m_clock;
    qint64 m_lastProbe = -2000;
    bool m_overloadReported = false;
    quint64 m_instance, m_sequence = 0;
    int m_httpClients = 0;
};
}
