// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include <QtTest>
#include <QBuffer>
#include <QJsonDocument>
#include <QJsonObject>
#include "ServiceDiagnostics.h"
#include "ComputerControlInterface.h"

class ServiceDiagnosticsTests : public QObject {
    Q_OBJECT
    struct DiagnosticReply : FeatureMessage {
        DiagnosticReply(const QString& id,const QByteArray& data):FeatureMessage(ServiceDiagnostics::uid(),ServiceDiagnostics::Reply) {
            addArgument(0,id);addArgument(1,data);
        }
    };
    ServiceDiagnostics::Snapshot sample() const {
        ServiceDiagnostics::Snapshot s;s.plugin="BuiltinUltraVncServer";s.internalPort=11200;
        s.backend=ServiceDiagnostics::Backend::Ready;s.frameAgeMs=1200;return s;
    }
    DiagnosticReply reply(const QString& id,const QByteArray& data) const {
        return DiagnosticReply(id,data);
    }
private Q_SLOTS:
    void initTestCase() {QLocale::setDefault(QLocale("en_US"));}
    void versionIsAnOptionalCompatibleSchemaOneExtension() {
        auto old=sample();const auto legacy=ServiceDiagnostics::encode(old);
        auto object=QJsonDocument::fromJson(legacy).object();QVERIFY(!object.contains("netlinkVersion"));
        ServiceDiagnostics::Snapshot parsed;parsed.netlinkVersion="stale";
        QVERIFY(ServiceDiagnostics::decode(legacy,&parsed));QVERIFY(parsed.netlinkVersion.isEmpty());
        QVERIFY(ServiceDiagnostics::tooltip(parsed).contains("NetLink client version: unavailable"));
        auto current=old;current.netlinkVersion="1.0.0-alpha.37";
        const auto encoded=ServiceDiagnostics::encode(current);auto extended=QJsonDocument::fromJson(encoded).object();
        QCOMPARE(extended.take("netlinkVersion").toString(),current.netlinkVersion);
        QCOMPARE(extended,object); // Every legacy field and schema remains identical.
        QVERIFY(ServiceDiagnostics::decode(encoded,&parsed));QCOMPARE(parsed.netlinkVersion,current.netlinkVersion);
        QVERIFY(ServiceDiagnostics::tooltip(parsed).contains("NetLink client version: 1.0.0-alpha.37"));
        object.insert("futureMetadata",QJsonObject{{"ignored",true}});
        QVERIFY(ServiceDiagnostics::decode(QJsonDocument(object).toJson(),&parsed));QVERIFY(parsed.netlinkVersion.isEmpty());
        for(const auto& value:QList<QJsonValue>{QJsonValue(),17,true,QJsonObject{},QString(""),QString(65,'1'),QString("1.0\ninvalid"),QString("1.0<script>"),QString("1.0")+QChar(0x202e)}) {
            object.insert("netlinkVersion",value);parsed.netlinkVersion="stale";
            QVERIFY(ServiceDiagnostics::decode(QJsonDocument(object).toJson(),&parsed));QVERIFY(parsed.netlinkVersion.isEmpty());QCOMPARE(parsed.internalPort,11200);
        }
    }
    void versionsArePerClientAndDisappearOnExpiryOrReconnect() {
        using namespace ServiceDiagnostics;ClientStatus first,second;first.reset(true);second.reset(true);
        auto a=sample(),b=sample();a.netlinkVersion="1.0.0-alpha.37";b.netlinkVersion="1.0.0-alpha.31";
        const auto one=first.request(0),two=second.request(0);
        QVERIFY(first.receive(reply(one,encode(a)),1));QVERIFY(second.receive(reply(two,encode(b)),1));
        QVERIFY(first.tooltip(1).contains(a.netlinkVersion));QVERIFY(!first.tooltip(1).contains(b.netlinkVersion));
        QVERIFY(second.tooltip(1).contains(b.netlinkVersion));QVERIFY(!second.tooltip(1).contains(a.netlinkVersion));
        QVERIFY(first.expire(35001));QVERIFY(!first.tooltip(35001).contains(a.netlinkVersion));
        second.reset(true);QVERIFY(!second.tooltip(2).contains(b.netlinkVersion));
        const auto old=second.request(2);QVERIFY(second.receive(reply(old,encode(sample())),3));
        QVERIFY(second.tooltip(3).contains("NetLink client version: unavailable"));
    }
    void queryAndReplyUseTheExistingBoundedFeatureEnvelope() {
        const auto id=QUuid::createUuid().toString(QUuid::WithoutBraces);
        FeatureMessage query(ServiceDiagnostics::uid(),ServiceDiagnostics::Query);query.addArgument(0,id).addArgument(1,1);
        QVERIFY(ServiceDiagnostics::validQuery(query));
        QBuffer buffer;QVERIFY(buffer.open(QIODevice::ReadWrite));QVERIFY(query.send(&buffer));buffer.seek(0);
        FeatureMessage parsed;QVERIFY(parsed.receive(&buffer));QVERIFY(ServiceDiagnostics::validQuery(parsed));
        QCOMPARE(parsed.argument(0).toString(),id);
        QVERIFY(!ServiceDiagnostics::validQuery(FeatureMessage(ServiceDiagnostics::uid(),ServiceDiagnostics::Reply).addArgument(0,id).addArgument(1,1)));
        QVERIFY(!ServiceDiagnostics::validQuery(FeatureMessage(ServiceDiagnostics::uid(),ServiceDiagnostics::Query).addArgument(0,"invalid").addArgument(1,1)));
        QVERIFY(!ServiceDiagnostics::validQuery(FeatureMessage(ServiceDiagnostics::uid(),ServiceDiagnostics::Query).addArgument(0,id.toUtf8()).addArgument(1,1)));
        QVERIFY(!ServiceDiagnostics::validQuery(FeatureMessage(ServiceDiagnostics::uid(),ServiceDiagnostics::Query).addArgument(0,id).addArgument(1,2)));
        QVERIFY(!ServiceDiagnostics::validQuery(FeatureMessage(ServiceDiagnostics::uid(),ServiceDiagnostics::Query).addArgument(0,id).addArgument(1,1).addArgument(2,0)));
        const auto data=ServiceDiagnostics::encode(sample());buffer.buffer().clear();buffer.seek(0);
        QVERIFY(reply(id,data).send(&buffer));buffer.seek(0);QVERIFY(parsed.receive(&buffer));
        ServiceDiagnostics::ClientStatus status;status.reset(true);const auto current=status.request(0);
        QVERIFY(status.receive(reply(current,parsed.argument(1).toByteArray()),1));
        QVERIFY(status.tooltip(1).contains("11200"));
    }
    void rejectsMalformedAndUnboundedRepliesWithoutPartialUpdates() {
        const auto encoded=ServiceDiagnostics::encode(sample());const auto good=QJsonDocument::fromJson(encoded).object();
        QList<QByteArray> bad{"",QByteArray(2049,'x'),"[]","{bad"};
        for(const auto& field:QStringList{"schema","plugin","external","internalPort","backend","frameAgeMs"}) {
            auto value=good;value.remove(field);bad.append(QJsonDocument(value).toJson());
        }
        const QList<QPair<QString,QJsonValue>> cases{{"schema",2},{"plugin",QString(101,'x')},{"plugin","bad\nplugin"},
            {"external",1},{"internalPort",65536},{"internalPort",-1},{"internalPort",11200.5},{"backend",5},{"frameAgeMs",-2},
            {"frameAgeMs",604800001},{"frameAgeMs",1.5}};
        for(const auto& entry:cases) {auto value=good;value[entry.first]=entry.second;bad.append(QJsonDocument(value).toJson());}
        for(const auto& data:bad) {auto value=sample();QVERIFY(!ServiceDiagnostics::decode(data,&value));QCOMPARE(value.plugin,sample().plugin);QCOMPARE(value.internalPort,11200);}
        auto value=sample();value.frameAgeMs=999999999;
        ServiceDiagnostics::Snapshot parsed;QVERIFY(ServiceDiagnostics::decode(ServiceDiagnostics::encode(value),&parsed));QCOMPARE(parsed.frameAgeMs,qint64(604800000));
    }
    void nonceExpiryBackoffAndReconnectDiscardStaleValues() {
        using namespace ServiceDiagnostics;
        ClientStatus status;QVERIFY(status.request(0).isEmpty());status.reset(true);
        const auto first=status.request(0);QVERIFY(!first.isEmpty());QVERIFY(status.request(1).isEmpty());
        QVERIFY(!status.receive(reply(QUuid::createUuid().toString(QUuid::WithoutBraces),encode(sample())),10));
        QVERIFY(status.receive(reply(first,encode(sample())),100));QCOMPARE(status.phase(),ClientStatus::Phase::Ready);
        QVERIFY(status.tooltip(1100).contains("2 s ago"));QVERIFY(status.request(15099).isEmpty());
        const auto second=status.request(15100);QVERIFY(!second.isEmpty());
        QVERIFY(!status.receive(reply(first,encode(sample())),15200));
        QVERIFY(status.expire(20100));QCOMPARE(status.phase(),ClientStatus::Phase::Unavailable);
        QVERIFY(!status.tooltip(20100).contains("11200"));QVERIFY(status.request(80099).isEmpty());
        const auto third=status.request(80100);QVERIFY(!third.isEmpty());
        status.reset();QVERIFY(!status.receive(reply(third,encode(sample())),80101));
        status.reset(true);const auto fourth=status.request(80102);QVERIFY(fourth!=third);
        QVERIFY(!status.receive(reply(third,encode(sample())),80103));
        QVERIFY(status.receive(reply(fourth,"invalid-json"),80104));QCOMPARE(status.phase(),ClientStatus::Phase::Invalid);
        QVERIFY(!status.tooltip(80104).contains("11200"));
        status.reset(true);const auto last=status.request(90000);
        QVERIFY(status.receive(reply(last,encode(sample())),90001));
        QVERIFY(!status.tooltip(125001).contains("11200"));QVERIFY(status.expire(125001));
    }
    void simultaneousClientsDoNotShareDiagnostics() {
        using namespace ServiceDiagnostics;ClientStatus first,second;first.reset(true);second.reset(true);
        const auto a=first.request(0),b=second.request(0);QVERIFY(a!=b);
        auto external=sample();external.plugin="ExternalVncServer";external.external=true;external.internalPort=5900;external.frameAgeMs=-1;
        QVERIFY(!first.receive(reply(b,encode(external)),1));QVERIFY(!second.receive(reply(a,encode(sample())),1));
        QVERIFY(first.receive(reply(a,encode(sample())),2));QVERIFY(second.receive(reply(b,encode(external)),2));
        QVERIFY(first.tooltip(2).contains("11200"));QVERIFY(!first.tooltip(2).contains("5900"));
        QVERIFY(second.tooltip(2).contains("5900"));QVERIFY(!second.tooltip(2).contains("11200"));
    }
    void lateAndWronglyTypedRepliesAreNeverAcceptedAsFresh() {
        using namespace ServiceDiagnostics;ClientStatus status;status.reset(true);
        auto id=status.request(100);QVERIFY(status.receive(reply(id,encode(sample())),5100));
        QCOMPARE(status.phase(),ClientStatus::Phase::Unavailable);
        status.reset(true);id=status.request(6000);
        FeatureMessage wrong(uid(),Reply);wrong.addArgument(0,id).addArgument(1,QString("not bytes"));
        QVERIFY(status.receive(wrong,6001));QCOMPARE(status.phase(),ClientStatus::Phase::Invalid);
    }
    void tooltipEscapesRemoteStringsAndDoesNotClaimVideoSuccess() {
        auto s=sample();s.plugin="<b>plugin & name</b>";s.external=true;s.backend=ServiceDiagnostics::Backend::Unreachable;s.frameAgeMs=-1;
        const auto text=ServiceDiagnostics::tooltip(s);
        QVERIFY(text.contains("&lt;b&gt;plugin &amp; name&lt;/b&gt;"));QVERIFY(!text.contains("<b>plugin"));
        QVERIFY(text.contains("No video frame observed"));QVERIFY(text.contains("VNC backend unavailable"));
        QVERIFY(text.contains("not proof of display"));QVERIFY(text.contains("external"));
    }
    void actualInterfaceStopAndReconnectRejectOldDiagnostics() {
        ComputerControlInterface ci{Computer()};
        QSignalSpy changed(&ci,&ComputerControlInterface::serviceDiagnosticsChanged);
        ci.m_state=ComputerControlInterface::State::Connected;ci.m_serviceDiagnostics.reset(true);
        const auto old=ci.m_serviceDiagnostics.request(ci.m_serviceDiagnosticsClock.elapsed());
        ci.handleFeatureMessage(reply(old,ServiceDiagnostics::encode(sample())));
        QVERIFY(ci.serviceDiagnosticsToolTip().contains("11200"));QCOMPARE(changed.count(),1);
        ci.stop();QVERIFY(!ci.serviceDiagnosticsToolTip().contains("11200"));QVERIFY(!ci.m_serviceDiagnosticsTimer.isActive());
        ci.handleFeatureMessage(reply(old,ServiceDiagnostics::encode(sample())));QCOMPARE(changed.count(),2);
        ci.m_state=ComputerControlInterface::State::Connected;ci.m_serviceDiagnostics.reset(true);
        const auto fresh=ci.m_serviceDiagnostics.request(ci.m_serviceDiagnosticsClock.elapsed());
        ci.handleFeatureMessage(reply(old,ServiceDiagnostics::encode(sample())));QVERIFY(!ci.serviceDiagnosticsToolTip().contains("11200"));
        ci.handleFeatureMessage(reply(fresh,ServiceDiagnostics::encode(sample())));QVERIFY(ci.serviceDiagnosticsToolTip().contains("11200"));
        ci.updateState();QVERIFY(!ci.serviceDiagnosticsToolTip().contains("11200"));
    }
};
QTEST_GUILESS_MAIN(ServiceDiagnosticsTests)
#include "ServiceDiagnosticsTests.moc"
