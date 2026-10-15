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
        auto object=QJsonDocument::fromJson(QByteArray::fromBase64(code.mid(4).toLatin1(),QByteArray::Base64UrlEncoding)).object();
        object[field]=QString::fromLatin1(QByteArray(32,'x').toBase64(QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals));
        return "NL1."+QString::fromLatin1(QJsonDocument(object).toJson(QJsonDocument::Compact).toBase64(QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals));
    }
private Q_SLOTS:
    void initTestCase() {QVERIFY2(QSslSocket::supportsSsl(),"TLS runtime is required for the diagnostic channel");}
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
};
QTEST_GUILESS_MAIN(DiagnosticLinkTests)
#include "DiagnosticLinkTests.moc"
