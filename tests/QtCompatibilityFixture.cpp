// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
// Run with the previous Qt runtime to generate PUBLIC, disposable test data.
// Never reads application settings or existing authentication keys.
#include <QCoreApplication>
#include <QBuffer>
#include <QDataStream>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>
#include <QtEndian>
#include <QtCrypto>

int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    QCA::Initializer crypto;
    if(app.arguments().size()!=2 || !QCA::isSupported("rsa")) return 1;
    auto key=QCA::KeyGenerator().createRSA(4096);
    if(key.isNull()) return 2;
    const QByteArray challenge(128,'x');
    const auto signature=key.signMessage(challenge,QCA::EMSA3_SHA512);
    if(signature.isEmpty()) return 3;
    const QString text=QString::fromUtf8("Studente È — student-01");
    const QUuid uid(QStringLiteral("{d13206b2-6de4-4a1d-aa1f-292c2f9911e8}"));
    QByteArray payload;
    QDataStream stream(&payload,QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_5_5);
    stream<<QVariant(1)<<QVariant(3)<<QVariant(challenge)<<QVariant(text)<<QVariant(uid);
    const quint32 length=qToBigEndian<quint32>(quint32(payload.size()));
    QByteArray frame(reinterpret_cast<const char*>(&length),sizeof(length));frame+=payload;
    const QJsonObject data{{QStringLiteral("qt"),QString::fromLatin1(qVersion())},
        {QStringLiteral("publicKey"),key.toPublicKey().toPEM()},
        {QStringLiteral("challenge"),QString::fromLatin1(challenge.toBase64())},
        {QStringLiteral("signature"),QString::fromLatin1(signature.toBase64())},
        {QStringLiteral("wire"),QString::fromLatin1(frame.toHex())},
        {QStringLiteral("text"),text},{QStringLiteral("uuid"),uid.toString()}};
    QFile output(app.arguments().at(1));
    if(!output.open(QIODevice::WriteOnly)) return 4;
    const auto bytes=QJsonDocument(data).toJson();
    return output.write(bytes)==bytes.size()?0:5;
}
