// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025-26 prof. Ing. Raffaele Mele
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include <QtTest>
#include <QBuffer>
#include <cstring>
#include "SocketDevice.h"
#include "VariantArrayMessage.h"
#include "FeatureMessage.h"
#include "RfbVeyonAuth.h"

struct ExactLengthInput {
    QByteArray bytes;
    qint64 offset = 0;
    int calls = 0;
    static qint64 dispatch(char* destination, qint64 size, SocketDevice::SocketOperation op, void* user) {
        auto& input=*static_cast<ExactLengthInput*>(user);
        if(op!=SocketDevice::SocketOpRead || size<0 || size>input.bytes.size()-input.offset) return 0;
        if(size) std::memcpy(destination,input.bytes.constData()+input.offset,size_t(size));
        input.offset+=size;++input.calls;return size;
    }
};

class TransportTests : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void authenticationTypesOnActualAdapter() {
        QBuffer output;output.open(QIODevice::ReadWrite);
        QVERIFY(VariantArrayMessage(&output).write(1).write(int(RfbVeyonAuth::KeyFile)).send());
        ExactLengthInput input{output.data()};SocketDevice adapter(ExactLengthInput::dispatch,&input);
        QCOMPARE(adapter.bytesAvailable(),qint64(0));
        VariantArrayMessage message(&adapter);QVERIFY(message.receive());
        QCOMPARE(message.read().toInt(),1);QCOMPARE(message.read().toInt(),int(RfbVeyonAuth::KeyFile));
        QCOMPARE(input.offset,qint64(output.size()));
    }
    void emptyAckAndChallengeRemainAligned() {
        QBuffer output;output.open(QIODevice::ReadWrite);
        QVERIFY(VariantArrayMessage(&output).send());
        const QByteArray challenge(128,'x');QVERIFY(VariantArrayMessage(&output).write(challenge).send());
        ExactLengthInput input{output.data()};SocketDevice adapter(ExactLengthInput::dispatch,&input);
        VariantArrayMessage ack(&adapter);QVERIFY(ack.receive());
        VariantArrayMessage message(&adapter);QVERIFY(message.receive());QCOMPARE(message.read().toByteArray(),challenge);
        QCOMPARE(input.offset,qint64(output.size()));
    }
    void featureMessageUsesSameBlockingAdapter() {
        const auto uid=QUuid::createUuid();FeatureMessage sent(uid,5);sent.addArgument(0,QString("test"));
        QBuffer output;output.open(QIODevice::ReadWrite);QVERIFY(sent.send(&output));
        ExactLengthInput input{output.data()};SocketDevice adapter(ExactLengthInput::dispatch,&input);
        FeatureMessage received;QVERIFY(received.receive(&adapter));QCOMPARE(received.featureUid(),uid);
        QCOMPARE(received.argument(0).toString(),QString("test"));
    }
    void bufferedFragmentsAreNotConsumed() {
        QBuffer output;output.open(QIODevice::ReadWrite);QVERIFY(VariantArrayMessage(&output).write(1).write(3).send());
        for(int size=0;size<output.size();++size) {
            QBuffer input;input.setData(output.data().left(size));input.open(QIODevice::ReadOnly);
            VariantArrayMessage message(&input);QVERIFY(!message.receive());QCOMPARE(input.pos(),qint64(0));
        }
    }
    void oversizedAdapterHeaderDoesNotReadBody() {
        const quint32 size=qToBigEndian<quint32>(VariantArrayMessage::MaxMessageSize+1);
        ExactLengthInput input{QByteArray(reinterpret_cast<const char*>(&size),sizeof(size))};
        SocketDevice adapter(ExactLengthInput::dispatch,&input);VariantArrayMessage message(&adapter);
        QVERIFY(!message.receive());QCOMPARE(input.calls,1);QCOMPARE(input.offset,qint64(4));
    }
    void truncatedBlockingBodyFails() {
        QBuffer output;output.open(QIODevice::ReadWrite);QVERIFY(VariantArrayMessage(&output).write(1).send());
        ExactLengthInput input{output.data().chopped(1)};SocketDevice adapter(ExactLengthInput::dispatch,&input);
        VariantArrayMessage message(&adapter);QVERIFY(!message.receive());QCOMPARE(input.offset,qint64(4));
    }
};
QTEST_GUILESS_MAIN(TransportTests)
#include "TransportTests.moc"
