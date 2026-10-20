// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include <QtTest>
#include <QBuffer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtCrypto>
#include "CryptoCore.h"
#include "VariantArrayMessage.h"

class QtRuntimeCompatibilityTests : public QObject {
    Q_OBJECT
    QCA::Initializer crypto;
    QJsonObject fixture;
private Q_SLOTS:
    void initTestCase() {
        QFile file(QString::fromUtf8(NETLINK_QT51518_FIXTURE));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto document=QJsonDocument::fromJson(file.readAll());
        QVERIFY(document.isObject());fixture=document.object();
        QCOMPARE(fixture.value("qt").toString(),QString("5.15.18"));
        QCOMPARE(QString::fromLatin1(qVersion()),QString::fromLatin1(QT_VERSION_STR));
        QVERIFY(QCA::isSupported("rsa"));
    }
    void verifiesPreviousRuntimeRsaChallengeAndRejectsChanges() {
        auto key=QCA::PublicKey::fromPEM(fixture.value("publicKey").toString());
        QVERIFY(!key.isNull());
        auto challenge=QByteArray::fromBase64(fixture.value("challenge").toString().toLatin1());
        const auto signature=QByteArray::fromBase64(fixture.value("signature").toString().toLatin1());
        QVERIFY(key.verifyMessage(challenge,signature,CryptoCore::DefaultSignatureAlgorithm));
        challenge[0]=char(challenge.at(0)^1);
        QVERIFY(!key.verifyMessage(challenge,signature,CryptoCore::DefaultSignatureAlgorithm));
        QVERIFY(!key.verifyMessage(challenge,QByteArray(),CryptoCore::DefaultSignatureAlgorithm));
    }
    void existingPemFormatAndAlgorithmsRoundTripWithoutApplicationKeys() {
        auto key=QCA::KeyGenerator().createRSA(CryptoCore::RsaKeySize);QVERIFY(!key.isNull());
        auto restored=QCA::PrivateKey::fromPEM(key.toPEM());QVERIFY(!restored.isNull());
        QCOMPARE(restored.toPublicKey().toDER(),key.toPublicKey().toDER());
        const QByteArray input("DISPOSABLE TEST CHALLENGE");
        QVERIFY(restored.toPublicKey().verifyMessage(input,key.signMessage(input,CryptoCore::DefaultSignatureAlgorithm),CryptoCore::DefaultSignatureAlgorithm));
        const auto encrypted=restored.toPublicKey().encrypt(input,CryptoCore::DefaultEncryptionAlgorithm);
        QCA::SecureArray plain;QVERIFY(restored.decrypt(encrypted,&plain,CryptoCore::DefaultEncryptionAlgorithm));
        QCOMPARE(plain.toByteArray(),input);
    }
    void preservesPreviousRuntimeWireBytesAndTypes() {
        const auto challenge=QByteArray::fromBase64(fixture.value("challenge").toString().toLatin1());
        const auto text=fixture.value("text").toString();const QUuid uid(fixture.value("uuid").toString());
        const auto expected=QByteArray::fromHex(fixture.value("wire").toString().toLatin1());
        QBuffer output;QVERIFY(output.open(QIODevice::ReadWrite));
        QVERIFY(VariantArrayMessage(&output).write(1).write(3).write(challenge).write(text).write(uid).send());
        QCOMPARE(output.data(),expected);
        QBuffer input;input.setData(expected);QVERIFY(input.open(QIODevice::ReadOnly));
        VariantArrayMessage message(&input);QVERIFY(message.receive());
        QCOMPARE(message.read().toInt(),1);QCOMPARE(message.read().toInt(),3);
        QCOMPARE(message.read().toByteArray(),challenge);QCOMPARE(message.read().toString(),text);
        QCOMPARE(message.read().toUuid(),uid);
    }
};
QTEST_GUILESS_MAIN(QtRuntimeCompatibilityTests)
#include "QtRuntimeCompatibilityTests.moc"
