// SPDX-License-Identifier: GPL-2.0-or-later
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include "SchoolDiscovery.h"
#include <QCryptographicHash>
#include <QHostInfo>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QNetworkReply>
#include <QRandomGenerator>
#include <QSysInfo>
#include <QUrl>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

namespace SchoolDiscovery {
static const QString Soap = "http://www.w3.org/2003/05/soap-envelope";
static const QString Addressing = "http://www.w3.org/2005/08/addressing";
static const QString Discovery = "http://docs.oasis-open.org/ws-dd/ns/discovery/2009/01";
static const QString School = "urn:veyon-school:discovery:1";
static const QString Target = "urn:docs-oasis-open-org:ws-dd:ns:discovery:2009:01";
static QString identifier() { return "urn:uuid:" + QUuid::createUuid().toString(QUuid::WithoutBraces); }
bool decode(const QByteArray& xml, Message& output) {
    output = {};
    if (xml.size() > 32768 || xml.isEmpty()) return false;
    QXmlStreamReader reader(xml);
    QStringList path;
    QList<QHash<QString, QString>> namespaceStack;
    Record record;
    QString text;
    int nodes = 0;
    bool envelope = false, body = false;
    while (!reader.atEnd()) {
        const auto token = reader.readNext();
        if (token == QXmlStreamReader::DTD || token == QXmlStreamReader::EntityReference) return false;
        if (token == QXmlStreamReader::StartElement) {
            if (++nodes > 512 || path.size() >= 16) return false;
            const auto name = reader.name().toString(), ns = reader.namespaceUri().toString();
            path.append(name); text.clear();
            auto namespaces = namespaceStack.isEmpty() ? QHash<QString,QString>{} : namespaceStack.last();
            for (const auto& declaration : reader.namespaceDeclarations())
                namespaces.insert(declaration.prefix().toString(), declaration.namespaceUri().toString());
            namespaceStack.append(namespaces);
            if (path.size() == 1) { if (name != "Envelope" || ns != Soap) return false; envelope = true; }
            if (path.size() == 2 && name == "Body" && ns == Soap) body = true;
            if (ns == Discovery && name == "AppSequence") {
                bool ok1 = false, ok2 = false;
                output.instance = reader.attributes().value("InstanceId").toULongLong(&ok1);
                output.sequence = reader.attributes().value("MessageNumber").toULongLong(&ok2);
                if (!ok1 || !ok2) return false;
            }
            if (ns == Discovery && name == "Scopes") output.matchBy = reader.attributes().value("MatchBy").toString();
            if (ns == Discovery && (name == "Hello" || name == "ProbeMatch" || name == "ResolveMatch" || name == "Bye")) record = {};
        } else if (token == QXmlStreamReader::Characters) {
            text += reader.text(); if (text.size() > 4096) return false;
        } else if (token == QXmlStreamReader::EndElement) {
            const auto name = reader.name().toString(), ns = reader.namespaceUri().toString();
            const auto value = text.trimmed();
            if (ns == Addressing && path.value(1) == "Header") {
                if (name == "Action") output.action = value;
                if (name == "MessageID") output.id = value;
                if (name == "RelatesTo") output.relatesTo = value;
                if (name == "To") output.to = value;
            }
            if (ns == Addressing && name == "Address" && path.value(1) == "Body") {
                record.endpoint = value;
                if (path.contains("Resolve")) output.resolve = value;
            }
            if (ns == Discovery && name == "Types") {
                // Resolve QName namespace, not an assumed prefix spelling.
                output.types.clear();
                for (const auto& type : value.split(' ', Qt::SkipEmptyParts)) {
                    const auto parts = type.split(':');
                    const auto uri = namespaceStack.last().value(parts.size()==2 ? parts[0] : QString{});
                    output.types += uri + '}' + parts.last() + ' ';
                }
                record.client = output.types.contains(School + "}Client ");
                record.proxy = output.types.contains(Discovery + "}DiscoveryProxy ");
            }
            if (ns == Discovery && name == "Scopes") { output.scopes = value; record.name = QUrl(value.section(' ',0,0)).path().section('/',-1); }
            if (ns == Discovery && name == "XAddrs") {
                const QUrl address(value.section(' ',0,0));
                if ((address.scheme() == "tcp" || (record.proxy && address.scheme()=="http")) && !QHostAddress(address.host()).isNull() && address.port() > 0 && address.port() <= 65535) {
                    record.address = address.host(); record.port = address.port();
                }
            }
            if (ns == Discovery && (name == "Hello" || name == "ProbeMatch" || name == "ResolveMatch" || name == "Bye")) {
                if (!record.endpoint.startsWith("urn:uuid:") || QUuid(record.endpoint.mid(9)).isNull()) return false;
                record.instance = output.instance; record.sequence = output.sequence;
                output.records.append(record); if (output.records.size() > 64) return false;
            }
            path.removeLast(); namespaceStack.removeLast(); text.clear();
        }
    }
    if (reader.hasError() || !envelope || !body || output.id.isEmpty() || output.id.size() > 256) return false;
    const QStringList actions = {"Hello", "Bye", "Probe", "ProbeMatches", "Resolve", "ResolveMatches"};
    const auto action = output.action.section('/',-1);
    return output.action == Discovery + '/' + action && actions.contains(action);
}
QByteArray encode(const Message& message) {
    QByteArray data; QXmlStreamWriter writer(&data);
    writer.writeStartDocument(); writer.writeStartElement("s:Envelope");
    writer.writeNamespace(Soap,"s"); writer.writeNamespace(Addressing,"a");
    writer.writeNamespace(Discovery,"d"); writer.writeNamespace(School,"v");
    writer.writeStartElement("s:Header");
    writer.writeTextElement("a:Action", Discovery + '/' + message.action);
    writer.writeTextElement("a:MessageID",message.id.isEmpty() ? identifier() : message.id);
    writer.writeTextElement("a:To",message.to.isEmpty() ? Target : message.to);
    if (!message.relatesTo.isEmpty()) writer.writeTextElement("a:RelatesTo",message.relatesTo);
    if (message.instance) {
        writer.writeStartElement("d:AppSequence"); writer.writeAttribute("InstanceId",QString::number(message.instance));
        writer.writeAttribute("MessageNumber",QString::number(message.sequence)); writer.writeEndElement();
    }
    writer.writeEndElement(); writer.writeStartElement("s:Body");
    writer.writeStartElement("d:"+message.action);
    auto endpoint = [&writer](const QString& id) { writer.writeStartElement("a:EndpointReference"); writer.writeTextElement("a:Address",id); writer.writeEndElement(); };
    if (message.action == "Probe") writer.writeTextElement("d:Types","v:Client");
    if (message.action == "Resolve") endpoint(message.resolve);
    for (const auto& record : message.records) {
        if (message.action.endsWith("Matches")) writer.writeStartElement(message.action == "ProbeMatches" ? "d:ProbeMatch" : "d:ResolveMatch");
        endpoint(record.endpoint);
        if (message.action != "Bye") {
            writer.writeTextElement("d:Types",record.proxy ? "d:DiscoveryProxy" : "v:Client");
            writer.writeTextElement("d:Scopes",School + "/host/" + QString::fromLatin1(QUrl::toPercentEncoding(record.name)));
            QUrl address; address.setScheme(record.proxy ? "http" : "tcp"); address.setHost(record.address); address.setPort(record.port);
            if(record.proxy) address.setPath("/discovery");
            if (!record.address.isEmpty()) writer.writeTextElement("d:XAddrs",address.toString());
            writer.writeTextElement("d:MetadataVersion",QString::number(message.instance ? message.instance : 1));
        }
        if (message.action.endsWith("Matches")) writer.writeEndElement();
    }
    writer.writeEndElement(); writer.writeEndElement(); writer.writeEndElement(); writer.writeEndDocument();
    return data;
}
Agent::Agent(bool master, const QString& bootstrap, int port, QObject* parent)
    : Agent(master,bootstrap,port,parent,true) {}
Agent::Agent(bool master, const QString& bootstrap, int port, QObject* parent, bool initializeSockets)
    : QObject(parent), m_master(master), m_bootstrap(bootstrap), m_port(port),
      m_udp4(this), m_udp6(this), m_probe4(this), m_probe6(this), m_http(this), m_network(this), m_timer(this),
      m_instance(quint64(QDateTime::currentSecsSinceEpoch())) {
    m_clock.start();
    auto identity = QSysInfo::machineUniqueId();
    if (identity.isEmpty()) identity = QHostInfo::localHostName().toUtf8();
    identity+=master ? ":discovery-proxy" : ":client";
    m_endpoint = "urn:uuid:" + QUuid::fromRfc4122(QCryptographicHash::hash(identity,QCryptographicHash::Sha256).left(16)).toString(QUuid::WithoutBraces);
    if(!initializeSockets) return;
    m_interfaces=QNetworkInterface::allInterfaces();
    const auto flags = QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint;
    if (!m_udp4.bind(QHostAddress::AnyIPv4,3702,flags)) m_error = m_udp4.errorString();
    m_udp6.bind(QHostAddress::AnyIPv6,3702,flags);
    m_udp4.setSocketOption(QAbstractSocket::MulticastTtlOption,1);
    m_udp6.setSocketOption(QAbstractSocket::MulticastTtlOption,1);
    for (const auto& interface : m_interfaces) {
        if (!(interface.flags() & QNetworkInterface::IsUp) || !(interface.flags() & QNetworkInterface::CanMulticast) || (interface.flags() & QNetworkInterface::IsLoopBack)) continue;
        m_udp4.joinMulticastGroup(QHostAddress("239.255.255.250"),interface);
        m_udp6.joinMulticastGroup(QHostAddress("ff02::c"),interface);
    }
    connect(&m_udp4,&QUdpSocket::readyRead,this,[this]{ readUdp(&m_udp4); });
    connect(&m_udp6,&QUdpSocket::readyRead,this,[this]{ readUdp(&m_udp6); });
    if (m_master) {
        m_probe4.bind(QHostAddress(QHostAddress::AnyIPv4),quint16(0));m_probe6.bind(QHostAddress(QHostAddress::AnyIPv6),quint16(0));
        m_probe4.setSocketOption(QAbstractSocket::MulticastTtlOption,1);m_probe6.setSocketOption(QAbstractSocket::MulticastTtlOption,1);
        connect(&m_probe4,&QUdpSocket::readyRead,this,[this]{readUdp(&m_probe4);});
        connect(&m_probe6,&QUdpSocket::readyRead,this,[this]{readUdp(&m_probe6);});
        if (!m_http.listen(QHostAddress::Any,3702)) m_error += " HTTP: " + m_http.errorString();
        connect(&m_http,&QTcpServer::newConnection,this,&Agent::acceptHttp);
    }
    connect(&m_timer,&QTimer::timeout,this,[this] {
        m_rate.clear();
        m_overloadReported=false;
        m_interfaces=QNetworkInterface::allInterfaces();
        const auto now = m_clock.elapsed();
        for (auto it=m_records.begin();it!=m_records.end();) { if (now-it->seen > 95000) it=m_records.erase(it); else ++it; }
        for (auto it=m_seen.begin();it!=m_seen.end();) { if (now-it.value()>60000) it=m_seen.erase(it); else ++it; }
        for (auto it=m_queries.begin();it!=m_queries.end();) {if(now-it.value()>60000) it=m_queries.erase(it);else ++it;}
        announce();
    });
    m_timer.start(30000);
    QTimer::singleShot(int(QRandomGenerator::global()->bounded(500u)),this,[this]{announce();if(m_master) probe();});
}
Agent::~Agent() { multicast(encode(announcement("Bye"))); }
QList<Record> Agent::records() const { return m_records.values(); }
Message Agent::announcement(const QString& action) const {
    Message message; message.action=action; message.id=identifier(); message.instance=m_instance; message.sequence=m_sequence+1;
    Record record; record.endpoint=m_endpoint; record.name=QHostInfo::localHostName(); record.port=m_master ? 3702 : m_port;
    record.client=!m_master;record.proxy=m_master;
    for(const auto& interface:m_interfaces) {
        if(!(interface.flags() & QNetworkInterface::IsUp) || (interface.flags() & QNetworkInterface::IsLoopBack)) continue;
        for(const auto& entry:interface.addressEntries()) {
            const auto address=entry.ip();
            if(!address.isLoopback() && address.protocol()==QAbstractSocket::IPv4Protocol) {record.address=address.toString();break;}
        }
        if(!record.address.isEmpty()) break;
    }
    message.records.append(record); return message;
}
void Agent::multicast(const QByteArray& data) {
    for (const auto& interface : m_interfaces) {
        if (!(interface.flags() & QNetworkInterface::IsUp) || !(interface.flags() & QNetworkInterface::CanMulticast) || (interface.flags() & QNetworkInterface::IsLoopBack)) continue;
        auto& udp4=m_master ? m_probe4 : m_udp4;
        auto& udp6=m_master ? m_probe6 : m_udp6;
        udp4.setMulticastInterface(interface); udp4.writeDatagram(data,QHostAddress("239.255.255.250"),3702);
        udp6.setMulticastInterface(interface); udp6.writeDatagram(data,QHostAddress("ff02::c"),3702);
    }
}
void Agent::announce() {
    auto message=announcement("Hello"); message.sequence=++m_sequence;
    multicast(encode(message));
    if (!m_master && !m_bootstrap.isEmpty() && m_bootstrap.size() <= 253) {
        QUrl url; url.setScheme("http"); url.setHost(m_bootstrap); url.setPort(3702); url.setPath("/discovery");
        message.to=url.toString();
        QNetworkRequest request(url); request.setHeader(QNetworkRequest::ContentTypeHeader,"application/soap+xml; charset=utf-8");
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
        auto reply=m_network.post(request,encode(message));
        QTimer::singleShot(5000,reply,&QNetworkReply::abort);
        connect(reply,&QNetworkReply::finished,reply,&QObject::deleteLater);
        reply->setReadBufferSize(32768);
    }
}
void Agent::probe() {
    const auto now=m_clock.elapsed();
    if(!m_master || m_queries.size()>=128 || now-m_lastProbe<2000) return;
    m_lastProbe=now;
    Message message;message.action="Probe";message.id=identifier();m_queries.insert(message.id,now);
    const auto data=encode(message);multicast(data);
    QTimer::singleShot(200,this,[this,data]{multicast(data);});QTimer::singleShot(600,this,[this,data]{multicast(data);});
}
QByteArray Agent::process(const QByteArray& xml,const QHostAddress& source,bool http) {
    const auto key=source.toString();
    if (source.isNull() || source.isMulticast()) return {};
    if(m_rate.size()>1024 || ++m_rate[key]>32) {
        if(!m_overloadReported) {m_overloadReported=true;qWarning("NetLink WS-Discovery input rate limited; further messages are temporarily ignored");}
        return {};
    }
    Message message; if(!decode(xml,message)) return {};
    if(m_seen.contains(message.id) || m_seen.size()>=4096) return {};
    m_seen.insert(message.id,m_clock.elapsed());
    const auto action=message.action.section('/',-1);
    if(action.endsWith("Matches") && m_master && !m_queries.contains(message.relatesTo)) return {};
    if(action=="Hello" || action=="ProbeMatches" || action=="ResolveMatches" || action=="Bye") {
        for(auto record : message.records) {
            if(action!="Bye" && !record.client) continue;
            // Never follow remote metadata URLs or accept an IP different from the sender.
            record.address=source.toString(); record.seen=m_clock.elapsed();
            auto old=m_records.constFind(record.endpoint);
            if(old!=m_records.cend() && (old->instance>record.instance || (old->instance==record.instance && old->sequence>=record.sequence && !http))) continue;
            if(action=="Bye") m_records.remove(record.endpoint);
            else if(m_master && (m_records.contains(record.endpoint) || m_records.size()<1024)) m_records.insert(record.endpoint,record);
        }
        return {};
    }
    if(action!="Probe" && action!="Resolve") return {};
    if(!message.scopes.isEmpty()) return {};
    const auto requestedType=message.types.trimmed();
    const bool proxyRequested=requestedType==Discovery+"}DiscoveryProxy" || (action=="Resolve" && message.resolve==m_endpoint);
    if(!requestedType.isEmpty() && requestedType!=School+"}Client" && !proxyRequested) return {};
    Message answer; answer.action=action=="Probe" ? "ProbeMatches" : "ResolveMatches";
    answer.relatesTo=message.id; answer.to=Addressing+"/anonymous";
    answer.instance=m_instance; answer.sequence=++m_sequence;
    if(!m_master && !proxyRequested) answer.records=announcement("Hello").records;
    else if(m_master && proxyRequested) answer.records=announcement("Hello").records;
    else if(m_master && http) answer.records=m_records.values();
    if(action=="Resolve") {
        QList<Record> selected; for(const auto& record:answer.records) if(record.endpoint==message.resolve) selected.append(record);
        answer.records=selected;
    }
    // Keep UDP/HTTP responses bounded. No reflection of arbitrary attacker-controlled addresses.
    if(answer.records.size()>64) answer.records=answer.records.mid(0,64);
    if(answer.records.isEmpty() && !http) return {};
    const auto data=encode(answer); return data.size()<=32768 ? data : QByteArray{};
}
void Agent::readUdp(QUdpSocket* socket) {
    int budget=16;
    QElapsedTimer slice;slice.start();
    while(socket->hasPendingDatagrams() && budget-- && slice.elapsed()<3) {
        const auto datagram=socket->receiveDatagram(32769);
        if(datagram.data().size()>32768) continue;
        const auto answer=process(datagram.data(),datagram.senderAddress(),false);
        if(!answer.isEmpty()) {
            const auto address=datagram.senderAddress(); const auto port=datagram.senderPort();
            QTimer::singleShot(int(QRandomGenerator::global()->bounded(500u)),this,[socket,address,port,answer]{socket->writeDatagram(answer,address,port);});
        }
    }
    // Coalesce continuations: repeated readyRead signals must not multiply the
    // queued drain callbacks under a multicast flood and starve the GUI.
    if(socket->hasPendingDatagrams() && !m_udpDrainScheduled.contains(socket)) {
        m_udpDrainScheduled.insert(socket);
        QTimer::singleShot(0,this,[this,socket]{m_udpDrainScheduled.remove(socket);readUdp(socket);});
    }
}
void Agent::acceptHttp() {
    while(auto socket=m_http.nextPendingConnection()) {
        if(m_httpClients>=16) { socket->abort(); socket->deleteLater(); continue; }
        ++m_httpClients; socket->setParent(this); socket->setReadBufferSize(40000);
        connect(socket,&QTcpSocket::disconnected,this,[this,socket]{--m_httpClients;socket->deleteLater();});
        QTimer::singleShot(5000,socket,&QTcpSocket::abort);
        connect(socket,&QTcpSocket::readyRead,this,[this,socket] {
            auto buffer=socket->property("httpBuffer").toByteArray()+socket->readAll();
            if(buffer.size()>40000) {socket->abort();return;}
            socket->setProperty("httpBuffer",buffer);
            const int end=buffer.indexOf("\r\n\r\n"); if(end<0) {if(buffer.size()>4096) socket->abort();return;}
            if(end>4096 || !buffer.startsWith("POST /discovery HTTP/1.") || socket->property("handled").toBool()) {socket->abort();return;}
            int length=-1; bool invalid=false;
            for(const auto& line:buffer.left(end).split('\n')) {
                const auto lower=line.trimmed().toLower();
                if(lower.startsWith("transfer-encoding:")) invalid=true;
                if(lower.startsWith("content-length:")) {bool ok=false; const int value=lower.mid(15).trimmed().toInt(&ok); if(!ok || length>=0) invalid=true; length=value;}
            }
            if(invalid || length<=0 || length>32768) {socket->abort();return;}
            if(buffer.size()<end+4+length) return;
            socket->setProperty("handled",true);
            const auto answer=process(buffer.mid(end+4,length),socket->peerAddress(),true);
            const auto headers=QByteArray("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: application/soap+xml\r\nContent-Length: ")+QByteArray::number(answer.size())+"\r\n\r\n";
            socket->write(headers+answer);socket->disconnectFromHost();
        });
    }
}
}
