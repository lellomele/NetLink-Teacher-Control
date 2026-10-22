// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include <QtTest>
#include <QBuffer>
#include <QTcpServer>
#include <QTcpSocket>
#include <cstring>
#include "VncProxyConnection.h"
#include "VncClientProtocol.h"

class TestClientProtocol : public VncClientProtocol {
public:
    explicit TestClientProtocol(QIODevice* device):VncClientProtocol(device,{}) {}
    void expectServerInit() {setState(FramebufferInit);}
};
class TestVideoProxy : public VncProxyConnection {
public:
    TestVideoProxy(QTcpSocket* socket):VncProxyConnection(socket,0,nullptr),protocol(&input) {}
    using VncProxyConnection::receiveServerMessage;
    void load(const QByteArray& bytes) {input.close();input.setData(bytes);input.open(QIODevice::ReadWrite);}
    bool initializeVideo() {
        rfbServerInitMsg init{};init.framebufferWidth=qToBigEndian<quint16>(1);init.framebufferHeight=qToBigEndian<quint16>(1);
        init.format.bitsPerPixel=32;init.format.depth=24;init.format.trueColour=1;
        init.format.redMax=qToBigEndian<quint16>(255);init.format.greenMax=qToBigEndian<quint16>(255);init.format.blueMax=qToBigEndian<quint16>(255);
        init.format.redShift=16;init.format.greenShift=8;
        load(QByteArray(reinterpret_cast<const char*>(&init),sz_rfbServerInitMsg));protocol.expectServerInit();return protocol.read();
    }
    VncClientProtocol& clientProtocol() override {return protocol;}
    VncServerProtocol& serverProtocol() override {qFatal("Server protocol must not be used by this isolated video test");}
    QBuffer input;
    TestClientProtocol protocol;
};
class ServerVideoStatusTests : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void readinessDoesNotClaimVideoAndOnlyActualFramesCount() {
        QTcpServer listener;QVERIFY(listener.listen(QHostAddress::LocalHost,0));auto outgoing=new QTcpSocket;
        outgoing->connectToHost(QHostAddress::LocalHost,listener.serverPort());QTRY_VERIFY(listener.hasPendingConnections());
        QScopedPointer<QTcpSocket> received(listener.nextPendingConnection());QVERIFY(received);
        TestVideoProxy proxy(outgoing);QVERIFY(!proxy.backendReady());QCOMPARE(proxy.videoFrameAge(),qint64(-1));
        QTcpServer backend;QVERIFY(backend.listen(QHostAddress::LocalHost,0));
        proxy.vncServerSocket()->connectToHost(QHostAddress::LocalHost,backend.serverPort());QTRY_VERIFY(backend.hasPendingConnections());
        QScopedPointer<QTcpSocket> backendSocket(backend.nextPendingConnection());QVERIFY(backendSocket);
        QTRY_COMPARE(proxy.vncServerSocket()->state(),QAbstractSocket::ConnectedState);QVERIFY(!proxy.backendReady());
        QVERIFY(proxy.initializeVideo());QVERIFY(proxy.backendReady());QCOMPARE(proxy.videoFrameAge(),qint64(-1));
        proxy.load(QByteArray(1,char(rfbBell)));QVERIFY(proxy.receiveServerMessage());QCOMPARE(proxy.videoFrameAge(),qint64(-1));
        proxy.load(QByteArray::fromHex("00000000"));QVERIFY(proxy.receiveServerMessage());QCOMPARE(proxy.videoFrameAge(),qint64(-1));
        const auto frame=QByteArray::fromHex("0000000100000000000100010000000011223344");
        proxy.load(frame.left(frame.size()-1));QVERIFY(!proxy.receiveServerMessage());QCOMPARE(proxy.videoFrameAge(),qint64(-1));
        proxy.load(frame);QVERIFY(proxy.receiveServerMessage());QVERIFY(proxy.videoFrameAge()>=0);
        QTRY_VERIFY(received->bytesAvailable()>=frame.size()+5);QVERIFY(received->readAll().endsWith(frame));
        QTest::qWait(20);QVERIFY(proxy.videoFrameAge()>=10);
        TestVideoProxy fresh(new QTcpSocket);QCOMPARE(fresh.videoFrameAge(),qint64(-1));
        // Each reply is scoped to its own proxy: activity on another Master's
        // connection must not make an idle/no-frame connection look healthy.
        const auto active=proxy.serviceDiagnostics("BuiltinUltraVncServer",false,11200,true,true);
        const auto idle=fresh.serviceDiagnostics("ExternalVncServer",true,5900,true,false);
        QVERIFY(active.frameAgeMs>=0);QCOMPARE(active.backend,ServiceDiagnostics::Backend::Ready);
        QCOMPARE(idle.frameAgeMs,qint64(-1));QCOMPARE(idle.backend,ServiceDiagnostics::Backend::Unreachable);
        QCOMPARE(idle.internalPort,5900);QVERIFY(idle.external);
    }
};
QTEST_GUILESS_MAIN(ServerVideoStatusTests)
#include "ServerVideoStatusTests.moc"
