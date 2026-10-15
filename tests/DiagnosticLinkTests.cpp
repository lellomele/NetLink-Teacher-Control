// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025-26 prof. Ing. Raffaele Mele
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include <QtTest>
#include <QSslSocket>
#include <QJsonDocument>
#include <QtEndian>
#include "DiagnosticLink.h"
#include "Diagnostics.h"

class DiagnosticLinkTests : public QObject {
    Q_OBJECT
    QJsonObject report() const {
        return {{"format","netlink-companion"},{"schema",1},{"identifiersIncluded",true},
            {"system",QJsonObject{{"computer","fixture"}}},{"settings",QJsonObject{}},
            {"interfaces",QJsonArray{}},{"services",QJsonArray{}},{"keys",QJsonArray{}}};
    }
    QString changed(const QString& code,const QString& field) {
        Companion::DiagnosticLink::Endpoint endpoint;if(!Companion::DiagnosticLink::decodeCode(code,endpoint)) return {};
        if(field=="pin") endpoint.pin=QByteArray(32,'x');else endpoint.token=QByteArray(32,'x');return Companion::DiagnosticLink::encodeCode(endpoint);
    }
private Q_SLOTS:
    void initTestCase() {QVERIFY2(QSslSocket::supportsSsl(),"TLS runtime is required for the diagnostic channel");}
    void compactCodeRetainsSecretsAndLegacyCompatibility() {
        for(const auto& address:{QString("192.0.2.25"),QString("2001:db8::25"),QString("fe80::25%12")}) {
            Companion::DiagnosticLink::Endpoint original{QHostAddress(address),11110,QByteArray(32,'p'),QByteArray(32,'t')},decoded;
            const auto compact=Companion::DiagnosticLink::encodeCode(original);QVERIFY(compact.startsWith("NL2."));if(original.address.protocol()==QAbstractSocket::IPv4Protocol) QCOMPARE(compact.size(),104);
            QVERIFY(Companion::DiagnosticLink::decodeCode(" \n"+compact.left(30)+"\r\n"+compact.mid(30)+" ",decoded));QCOMPARE(decoded.address,original.address);QCOMPARE(decoded.port,original.port);QCOMPARE(decoded.pin,original.pin);QCOMPARE(decoded.token,original.token);
            const auto base64=[](const QByteArray& bytes) {return QString::fromLatin1(bytes.toBase64(QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals));};
            const auto legacy="NL1."+base64(QJsonDocument(QJsonObject{{"ip",address},{"port",11110},{"pin",base64(original.pin)},{"token",base64(original.token)}}).toJson(QJsonDocument::Compact));
            QVERIFY(compact.size()*10<legacy.size()*7);QVERIFY(Companion::DiagnosticLink::decodeCode(legacy,decoded));QCOMPARE(decoded.pin,original.pin);QCOMPARE(decoded.token,original.token);
            auto damaged=compact;damaged[20]=damaged[20]=='x'?'y':'x';QVERIFY(!Companion::DiagnosticLink::decodeCode(damaged,decoded));QVERIFY(decoded.token.isEmpty());
            for(int n=4;n<compact.size();++n) QVERIFY(!Companion::DiagnosticLink::decodeCode(compact.left(n),decoded));
        }
    }
    void invalidCodesAndReports() {
        Companion::DiagnosticLink link;Companion::DiagnosticLink::Endpoint endpoint;
        for(const auto& value:{QString(),QString("NL1.bad"),QString(1025,'x'),QString("http://host")}) QVERIFY(!Companion::DiagnosticLink::decodeCode(value,endpoint));
        QSignalSpy errors(&link,&Companion::DiagnosticLink::failed);QVERIFY(!link.sendReport("NL1.bad",report()));QCOMPARE(errors.size(),1);
        QVERIFY(!link.receiveOnce(QHostAddress::Any));
    }
    void encryptedOneShotReport() {
        Companion::DiagnosticLink receiver,sender;QSignalSpy received(&receiver,&Companion::DiagnosticLink::reportReceived),sent(&sender,&Companion::DiagnosticLink::reportSent),errors(&sender,&Companion::DiagnosticLink::failed);
        QVERIFY(receiver.receiveOnce(QHostAddress::LocalHost));const auto code=receiver.pairingCode();
        Companion::DiagnosticLink::Endpoint endpoint;QVERIFY(Companion::DiagnosticLink::decodeCode(code,endpoint));QCOMPARE(endpoint.address,QHostAddress(QHostAddress::LocalHost));QVERIFY(endpoint.port>0);QCOMPARE(endpoint.pin.size(),32);QCOMPARE(endpoint.token.size(),32);
        QVERIFY(sender.sendReport(code,report()));QTRY_COMPARE_WITH_TIMEOUT(received.size(),1,8000);QTRY_COMPARE_WITH_TIMEOUT(sent.size(),1,3000);QCOMPARE(errors.size(),0);QCOMPARE(qvariant_cast<QJsonObject>(received[0][0]),report());QVERIFY(receiver.pairingCode().isEmpty());
        Companion::DiagnosticLink replay;QSignalSpy replayErrors(&replay,&Companion::DiagnosticLink::failed);QVERIFY(replay.sendReport(code,report()));QTRY_VERIFY_WITH_TIMEOUT(!replayErrors.isEmpty(),8000);QCOMPARE(received.size(),1);
    }
    void wrongPinDoesNotDeliverData() {
        Companion::DiagnosticLink receiver,sender;QVERIFY(receiver.receiveOnce(QHostAddress::LocalHost));QSignalSpy received(&receiver,&Companion::DiagnosticLink::reportReceived),errors(&sender,&Companion::DiagnosticLink::failed),sent(&sender,&Companion::DiagnosticLink::reportSent);
        QVERIFY(sender.sendReport(changed(receiver.pairingCode(),"pin"),report()));QTRY_VERIFY_WITH_TIMEOUT(!errors.isEmpty(),8000);QCOMPARE(received.size(),0);QCOMPARE(sent.size(),0);
    }
    void wrongTokenDoesNotDeliverData() {
        Companion::DiagnosticLink receiver,sender;QVERIFY(receiver.receiveOnce(QHostAddress::LocalHost));QSignalSpy received(&receiver,&Companion::DiagnosticLink::reportReceived),errors(&sender,&Companion::DiagnosticLink::failed);
        QVERIFY(sender.sendReport(changed(receiver.pairingCode(),"token"),report()));QTRY_VERIFY_WITH_TIMEOUT(!errors.isEmpty(),8000);QCOMPARE(received.size(),0);QVERIFY(!receiver.pairingCode().isEmpty());
    }
    void expiryAndExplicitStop() {
        Companion::DiagnosticLink receiver;QSignalSpy errors(&receiver,&Companion::DiagnosticLink::failed);QVERIFY(receiver.receiveOnce(QHostAddress::LocalHost,0,30));QTRY_VERIFY_WITH_TIMEOUT(receiver.pairingCode().isEmpty(),1000);QVERIFY(!errors.isEmpty());
        QVERIFY(receiver.receiveOnce(QHostAddress::LocalHost));receiver.stop();QVERIFY(receiver.pairingCode().isEmpty());
    }
    void oversizedUnauthenticatedFrameIsRejected() {
        Companion::DiagnosticLink receiver;QVERIFY(receiver.receiveOnce(QHostAddress::LocalHost));
        Companion::DiagnosticLink::Endpoint endpoint;QVERIFY(Companion::DiagnosticLink::decodeCode(receiver.pairingCode(),endpoint));
        QSignalSpy received(&receiver,&Companion::DiagnosticLink::reportReceived),errors(&receiver,&Companion::DiagnosticLink::failed);
        QSslSocket attacker;attacker.setPeerVerifyMode(QSslSocket::VerifyNone);attacker.setProtocol(QSsl::TlsV1_2OrLater);attacker.connectToHostEncrypted("127.0.0.1",endpoint.port);
        QTRY_VERIFY_WITH_TIMEOUT(attacker.isEncrypted(),8000);
        const auto size=qToBigEndian<quint32>(4097);QCOMPARE(attacker.write(reinterpret_cast<const char*>(&size),4),qint64(4));
        QTRY_VERIFY_WITH_TIMEOUT(!errors.isEmpty(),3000);QCOMPARE(received.size(),0);QVERIFY(!receiver.pairingCode().isEmpty());
    }
    void stopCallbackMayDestroyLink() {
        QPointer<Companion::DiagnosticLink> receiver=new Companion::DiagnosticLink;
        connect(receiver,&Companion::DiagnosticLink::stopped,this,[&receiver]{delete receiver.data();});
        QVERIFY(!receiver->receiveOnce(QHostAddress::LocalHost));QVERIFY(receiver.isNull());
        QPointer<Companion::DiagnosticLink> sender=new Companion::DiagnosticLink;
        connect(sender,&Companion::DiagnosticLink::stopped,this,[&sender]{delete sender.data();});
        QVERIFY(!sender->sendReport("NL1.bad",report()));QVERIFY(sender.isNull());
    }
    void receivedCallbackMayDestroyReceiver() {
        QPointer<Companion::DiagnosticLink> receiver=new Companion::DiagnosticLink;
        Companion::DiagnosticLink sender;
        QVERIFY(receiver->receiveOnce(QHostAddress::LocalHost));const auto code=receiver->pairingCode();
        connect(receiver,&Companion::DiagnosticLink::reportReceived,this,[&receiver](const QJsonObject&){delete receiver.data();});
        QVERIFY(sender.sendReport(code,report()));QTRY_VERIFY_WITH_TIMEOUT(receiver.isNull(),8000);
        QTest::qWait(50); // deliver pending disconnect/deferred parser events
    }
    void stoppedCallbackMayStartNewSessionWithoutOuterCallOverwritingIt() {
        for(bool outerSend:{false,true}) {
            Companion::DiagnosticLink receiver,sender;
            QString fresh;bool started=false;QMetaObject::Connection restart;
            restart=connect(&receiver,&Companion::DiagnosticLink::stopped,this,[&]{
                disconnect(restart);
                started=receiver.receiveOnce(QHostAddress::LocalHost);
                fresh=receiver.pairingCode();
            });
            const auto outerResult=outerSend?receiver.sendReport("NL1.bad",report()):receiver.receiveOnce(QHostAddress::LocalHost);
            QVERIFY(!outerResult);QVERIFY(started);QVERIFY(!fresh.isEmpty());QCOMPARE(receiver.pairingCode(),fresh);
            QSignalSpy received(&receiver,&Companion::DiagnosticLink::reportReceived),sent(&sender,&Companion::DiagnosticLink::reportSent);
            QVERIFY(sender.sendReport(fresh,report()));
            QTRY_COMPARE_WITH_TIMEOUT(received.size(),1,8000);QTRY_COMPARE_WITH_TIMEOUT(sent.size(),1,3000);
        }
    }
    void failureCallbackMayDestroySender() {
        Companion::DiagnosticLink receiver;QVERIFY(receiver.receiveOnce(QHostAddress::LocalHost));
        QPointer<Companion::DiagnosticLink> sender=new Companion::DiagnosticLink;
        connect(sender,&Companion::DiagnosticLink::failed,this,[&sender](const QString&){delete sender.data();});
        QVERIFY(sender->sendReport(changed(receiver.pairingCode(),"pin"),report()));
        QTRY_VERIFY_WITH_TIMEOUT(sender.isNull(),8000);QCoreApplication::processEvents();
    }
    void queuedNotificationsDoNotCrossSessionGenerations() {
        Companion::DiagnosticLink link;
        QSignalSpy received(&link,&Companion::DiagnosticLink::reportReceived),sent(&link,&Companion::DiagnosticLink::reportSent),failed(&link,&Companion::DiagnosticLink::failed);
        link.postFailure("old failure");link.postReportReceived(report());link.postReportSent();
        QVERIFY(link.receiveOnce(QHostAddress::LocalHost));const auto fresh=link.pairingCode();
        QCoreApplication::processEvents();
        QCOMPARE(received.size(),0);QCOMPARE(sent.size(),0);QCOMPARE(failed.size(),0);QCOMPARE(link.pairingCode(),fresh);
        link.postReportSent();QTRY_COMPARE_WITH_TIMEOUT(sent.size(),1,1000);
    }
};
QTEST_GUILESS_MAIN(DiagnosticLinkTests)
#include "DiagnosticLinkTests.moc"
