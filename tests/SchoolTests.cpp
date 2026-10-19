// SPDX-License-Identifier: GPL-2.0-or-later
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include <QtTest>
#include <QBuffer>
#include <QRandomGenerator>
#include <QtEndian>
#include <QDir>
#include <QFile>
#include <QNetworkProxy>
#include <QNetworkDatagram>
#include <QNetworkReply>
#include "FeatureMessage.h"
#include "SchoolDiscovery.h"
#include "SchoolNetworkPolicy.h"
#include "VariantArrayMessage.h"

class SchoolTests : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void addressValidation() {
        for(const auto& value:{"192.168.1.25","10.2.0.0/24","2001:db8::5","2001:db8::/64"}) QVERIFY(SchoolNetwork::validAddress(value));
        for(const auto& value:{"0.0.0.0","::","127.0.0.1","::1","224.1.2.3","ff02::1","10.1.1.1/0","10.0.0.1/33","::/129","a.example","10.0.0.1/24/1","fe80::1%eth0"}) QVERIFY(!SchoolNetwork::validAddress(value));
    }
    void fragmentedFrames() {
        QBuffer sender;sender.open(QIODevice::ReadWrite);
        const auto id=QUuid::createUuid();FeatureMessage outgoing(id,5);outgoing.addArgument(0,QString("Anna Rossi"));QVERIFY(outgoing.send(&sender));
        const auto data=sender.data();
        for(int size=0;size<data.size();++size) {
            QBuffer fragment;fragment.setData(data.left(size));fragment.open(QIODevice::ReadOnly);
            FeatureMessage message;QVERIFY(!message.receive(&fragment));QCOMPARE(fragment.pos(),qint64(0));
        }
        QBuffer receiver;receiver.setData(data);receiver.open(QIODevice::ReadOnly);
        FeatureMessage message;QVERIFY(message.receive(&receiver));QCOMPARE(message.featureUid(),id);QCOMPARE(message.command(),5);QCOMPARE(message.argument(0).toString(),QString("Anna Rossi"));
    }
    void oversizedFrame() {
        quint32 size=qToBigEndian<quint32>(0xffffffffu);
        QBuffer receiver;receiver.setData(QByteArray(reinterpret_cast<char*>(&size),sizeof(size)));receiver.open(QIODevice::ReadOnly);
        FeatureMessage message;QVERIFY(!message.isReadyForReceive(&receiver));QVERIFY(!receiver.isOpen());
    }
    void invalidMessageTypes() {
        QBuffer sender;sender.open(QIODevice::ReadWrite);VariantArrayMessage(&sender).write(QString("not-a-uuid")).write(5).write(QVariantMap{}).send();
        sender.seek(0);FeatureMessage message;QVERIFY(!message.receive(&sender));
    }
    void discoveryRoundTrip() {
        SchoolDiscovery::Message input;input.action="Hello";input.id="urn:uuid:"+QUuid::createUuid().toString(QUuid::WithoutBraces);input.instance=123;input.sequence=9;
        SchoolDiscovery::Record record;record.endpoint="urn:uuid:"+QUuid::createUuid().toString(QUuid::WithoutBraces);record.name="PC & Aula <1>";record.address="192.168.25.42";record.port=11100;input.records.append(record);
        const auto xml=SchoolDiscovery::encode(input);SchoolDiscovery::Message output;QVERIFY(SchoolDiscovery::decode(xml,output));
        QCOMPARE(output.records.size(),1);QCOMPARE(output.records[0].endpoint,record.endpoint);QCOMPARE(output.records[0].address,record.address);QCOMPARE(output.records[0].name,record.name);QVERIFY(output.records[0].client);QCOMPARE(output.sequence,quint64(9));
    }
    void discoveryRejectsDtd() {
        SchoolDiscovery::Message message;QVERIFY(!SchoolDiscovery::decode("<!DOCTYPE test [<!ENTITY xx 'test'>]><test>&xx;</test>",message));
        QVERIFY(!SchoolDiscovery::decode(QByteArray(32769,'x'),message));
        QVERIFY(!SchoolDiscovery::decode("<Envelope xmlns='wrong'/>",message));
    }
    void discoveryProbe() {
        SchoolDiscovery::Message input;input.action="Probe";input.id="urn:uuid:"+QUuid::createUuid().toString(QUuid::WithoutBraces);
        SchoolDiscovery::Message output;QVERIFY(SchoolDiscovery::decode(SchoolDiscovery::encode(input),output));
        QVERIFY(output.types.contains("urn:veyon-school:discovery:1}Client"));
    }
    void discoveryCodecSupportsBothFamiliesWithoutAdvertisingLocalZoneNumbers() {
        SchoolDiscovery::Message message;message.action="Hello";message.instance=1;message.sequence=1;
        SchoolDiscovery::Record record;record.endpoint="urn:uuid:"+QUuid::createUuid().toString(QUuid::WithoutBraces);record.name="Dual stack";record.port=11123;
        record.addresses=QStringList{"192.0.2.25","2001:db8::25","fe80::25%17"};message.records.append(record);
        const auto xml=SchoolDiscovery::encode(message);QVERIFY(!xml.contains("%17"));QVERIFY(!xml.contains("%2517"));
        SchoolDiscovery::Message parsed;QVERIFY(SchoolDiscovery::decode(xml,parsed));QCOMPARE(parsed.records.size(),1);
        QCOMPARE(parsed.records.first().port,11123);QCOMPARE(parsed.records.first().addresses,QStringList({"192.0.2.25","2001:db8::25","fe80::25"}));
        record.addresses=QStringList{"2001:db8::25"};message.records={record};QVERIFY(SchoolDiscovery::decode(SchoolDiscovery::encode(message),parsed));
        QCOMPARE(parsed.records.first().address,QString("2001:db8::25"));QCOMPARE(parsed.records.first().port,11123);
    }
    void independentProbeFixture() {
        const QByteArray fixture("<s:Envelope xmlns:s='http://www.w3.org/2003/05/soap-envelope' xmlns:w='http://www.w3.org/2005/08/addressing' xmlns:dis='http://docs.oasis-open.org/ws-dd/ns/discovery/2009/01' xmlns:school='urn:veyon-school:discovery:1'>"
            "<s:Header><w:Action>http://docs.oasis-open.org/ws-dd/ns/discovery/2009/01/Probe</w:Action><w:MessageID>urn:uuid:0a6dc791-2be6-4991-9af1-454778a1917a</w:MessageID><w:To>urn:docs-oasis-open-org:ws-dd:ns:discovery:2009:01</w:To></s:Header>"
            "<s:Body><dis:Probe><dis:Types>school:Client</dis:Types></dis:Probe></s:Body></s:Envelope>");
        SchoolDiscovery::Message message;QVERIFY(SchoolDiscovery::decode(fixture,message));
        QCOMPARE(message.action,QString("http://docs.oasis-open.org/ws-dd/ns/discovery/2009/01/Probe"));
        QCOMPARE(message.types.trimmed(),QString("urn:veyon-school:discovery:1}Client"));
    }
    void discoveryHttpLoopback() {
        // Exercise the real HTTP handler on an ephemeral loopback port, with
        // no production listener, multicast traffic or installed service.
        SchoolDiscovery::Agent master(true,{},11100,this,false);
        QVERIFY(master.m_http.listen(QHostAddress::LocalHost,0));
        QObject::connect(&master.m_http,&QTcpServer::newConnection,&master,&SchoolDiscovery::Agent::acceptHttp);
        QNetworkAccessManager network;network.setProxy(QNetworkProxy(QNetworkProxy::NoProxy));
        QNetworkRequest request(QUrl(QString("http://127.0.0.1:%1/discovery").arg(master.m_http.serverPort())));
        request.setHeader(QNetworkRequest::ContentTypeHeader,"application/soap+xml; charset=utf-8");
        SchoolDiscovery::Record local;local.endpoint="urn:uuid:"+QUuid::createUuid().toString(QUuid::WithoutBraces);
        local.name="Local test client";local.address="127.0.0.1";local.port=11100;local.client=true;
        for(const auto& action:{"Hello","Probe"}) {
            SchoolDiscovery::Message packet;packet.action=action;packet.instance=1;packet.sequence=1;
            if(packet.action=="Hello") packet.records.append(local);
            QScopedPointer<QNetworkReply> reply(network.post(request,SchoolDiscovery::encode(packet)));
            QTRY_VERIFY_WITH_TIMEOUT(reply->isFinished(),7000);
            QCOMPARE(reply->error(),QNetworkReply::NoError);
            QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),200);
            const auto body=reply->readAll();
            if(packet.action=="Hello") QVERIFY(body.isEmpty());
            else {SchoolDiscovery::Message answer;QVERIFY(SchoolDiscovery::decode(body,answer));QCOMPARE(answer.action.section('/',-1),QString("ProbeMatches"));}
            QVERIFY(master.records().isEmpty());
        }
    }
    void schemaPackets() {
        const auto directory=qEnvironmentVariable("VEYON_SCHOOL_SCHEMA_FIXTURES");if(directory.isEmpty()) QSKIP("Schema fixture export not requested");
        QDir().mkpath(directory);
        SchoolDiscovery::Record record;record.endpoint="urn:uuid:98190dc2-0890-4ef8-ac9a-5940995e6119";record.name="Lab01";record.address="192.0.2.25";record.port=11100;
        record.addresses=QStringList{record.address,"2001:db8::25","fe80::25%17"};
        for(const auto& action:{"Hello","Bye","Probe","ProbeMatches","Resolve","ResolveMatches"}) {
            SchoolDiscovery::Message message;message.action=action;message.instance=100;message.sequence=1;message.resolve=record.endpoint;
            if(message.action!="Probe" && message.action!="Resolve") message.records.append(record);
            if(message.action.endsWith("Matches")) message.relatesTo="urn:uuid:0a6dc791-2be6-4991-9af1-454778a1917a";
            QFile file(QDir(directory).filePath(message.action+".xml"));QVERIFY(file.open(QIODevice::WriteOnly));QVERIFY(file.write(SchoolDiscovery::encode(message))>0);
        }
    }
    void deterministicMalformedInputs() {
        QRandomGenerator random(20261014);SchoolDiscovery::Message message;
        for(int iteration=0;iteration<10000;++iteration) {
            QByteArray input(int(random.bounded(1024u)),'\0');for(auto& byte:input) byte=char(random.generate()&255);
            QVERIFY(!SchoolDiscovery::decode(input,message));
        }
    }
    void discoveryRealIpv6UdpProbeOnLoopback() {
        SchoolDiscovery::Agent agent(false,{},11100,this,false);
        if(!agent.m_udp6.bind(QHostAddress(QHostAddress::LocalHostIPv6),quint16(0))) QSKIP("IPv6 loopback is disabled on this test host");
        connect(&agent.m_udp6,&QUdpSocket::readyRead,&agent,[&agent]{agent.readUdp(&agent.m_udp6);});
        QUdpSocket requester;QVERIFY(requester.bind(QHostAddress(QHostAddress::LocalHostIPv6),quint16(0)));
        SchoolDiscovery::Message probe;probe.action="Probe";probe.id="urn:uuid:"+QUuid::createUuid().toString(QUuid::WithoutBraces);
        const auto bytes=SchoolDiscovery::encode(probe);
        QCOMPARE(requester.writeDatagram(bytes,QHostAddress::LocalHostIPv6,agent.m_udp6.localPort()),qint64(bytes.size()));
        QTRY_VERIFY_WITH_TIMEOUT(requester.hasPendingDatagrams(),2000);
        const auto datagram=requester.receiveDatagram();SchoolDiscovery::Message answer;
        QVERIFY(SchoolDiscovery::decode(datagram.data(),answer));QCOMPARE(answer.relatesTo,probe.id);
        QCOMPARE(answer.records.size(),1);QCOMPARE(answer.records.first().endpoint,agent.m_endpoint);
        QCOMPARE(datagram.senderAddress(),QHostAddress(QHostAddress::LocalHostIPv6));
    }
};
QTEST_GUILESS_MAIN(SchoolTests)
#include "SchoolTests.moc"
