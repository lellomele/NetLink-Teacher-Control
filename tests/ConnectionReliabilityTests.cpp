// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025-26 prof. Ing. Raffaele Mele
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <atomic>
#include <algorithm>
#include <numeric>
#include <thread>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wparentheses"
#include <rfb/rfbclient.h>
#pragma GCC diagnostic pop
#include "Configuration/JsonStore.h"
#include "VeyonConfiguration.h"
#include "VncConnection.h"
#include "VncEvents.h"
#include "ComputerControlInterface.h"
#include "HostAddress.h"
#include "VeyonConnection.h"
#include "AuthenticationProxy.h"

class ConnectionReliabilityTests : public QObject
{
    Q_OBJECT
    QCA::Initializer m_cryptoInitializer;
    QTemporaryDir m_directory;
    Configuration::JsonStore m_store{Configuration::Store::System, m_directory.filePath("connection.json")};
    VeyonConfiguration m_configuration{&m_store};

    class CountingEvent : public VncEvent
    {
        int& m_destroyed;
        int& m_fired;
    public:
        CountingEvent(int& destroyed, int& fired):m_destroyed(destroyed),m_fired(fired){}
        ~CountingEvent() override {++m_destroyed;}
        void fire(rfbClient*) override {++m_fired;}
    };

private Q_SLOTS:
    void eachConnectionOwnsAnImmutableKeyAndProxyTokensStillTakePriority() {
        QObject lifetime;
        auto first=new VeyonConnection(new VncConnection(m_configuration),RfbVeyonAuth::KeyFile,&lifetime);
        auto second=new VeyonConnection(new VncConnection(m_configuration),RfbVeyonAuth::KeyFile,&lifetime);
        AuthenticationCredentials a,b;a.setAuthenticationKeyName("lab_a");b.setAuthenticationKeyName("lab_b");
        CryptoCore::KeyGenerator generator;QVERIFY(a.setPrivateKey(generator.createRSA(2048)));QVERIFY(b.setPrivateKey(generator.createRSA(2048)));
        QVERIFY(first->setAuthenticationCredentials(a));QVERIFY(second->setAuthenticationCredentials(b));
        a.setAuthenticationKeyName("changed_after_copy");
        QCOMPARE(first->authenticationCredentials().authenticationKeyName(),QString("lab_a"));
        QCOMPARE(second->authenticationCredentials().authenticationKeyName(),QString("lab_b"));
        QVERIFY(!first->setAuthenticationCredentials(b));
        const auto challenge=CryptoCore::generateChallenge();
        auto selectedA=first->authenticationCredentials().privateKey(),selectedB=second->authenticationCredentials().privateKey();
        const auto signatureA=selectedA.signMessage(challenge,CryptoCore::DefaultSignatureAlgorithm),signatureB=selectedB.signMessage(challenge,CryptoCore::DefaultSignatureAlgorithm);
        auto publicA=a.privateKey().toPublicKey(),publicB=b.privateKey().toPublicKey();
        QVERIFY(publicA.verifyMessage(challenge,signatureA,CryptoCore::DefaultSignatureAlgorithm));QVERIFY(publicB.verifyMessage(challenge,signatureB,CryptoCore::DefaultSignatureAlgorithm));
        QVERIFY(!publicA.verifyMessage(challenge,signatureB,CryptoCore::DefaultSignatureAlgorithm));QVERIFY(!publicB.verifyMessage(challenge,signatureA,CryptoCore::DefaultSignatureAlgorithm));
        std::atomic<bool> isolated{true};std::thread threadA([&]{for(int i=0;i<10000;++i) if(first->authenticationCredentials().authenticationKeyName()!=QString("lab_a")) isolated=false;});
        std::thread threadB([&]{for(int i=0;i<10000;++i) if(second->authenticationCredentials().authenticationKeyName()!=QString("lab_b")) isolated=false;});
        threadA.join();threadB.join();QVERIFY(isolated.load());
        class TokenProxy : public AuthenticationProxy {RfbVeyonAuth::Type initCredentials() override {return RfbVeyonAuth::Token;}};
        auto proxy=new TokenProxy;AuthenticationCredentials token;token.setAuthenticationKeyName("proxy_credentials");proxy->setCredentials(token);
        first->setAuthenticationProxy(proxy);QCOMPARE(first->authenticationCredentials().authenticationKeyName(),QString("proxy_credentials"));
        auto failed=new VeyonConnection(new VncConnection(m_configuration),RfbVeyonAuth::KeyFile,&lifetime);
        QVERIFY(failed->setAuthenticationCredentials({}));QVERIFY(failed->authenticationCredentials().authenticationKeyName().isEmpty());
        QPointer<VeyonConnection> pointer(first);first->stopAndDeleteLater();QVERIFY(!first->setAuthenticationCredentials(b));
        second->stopAndDeleteLater();failed->stopAndDeleteLater();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QVERIFY(pointer.isNull());
        QVERIFY(!VeyonCore::instance());
    }
    void disconnectionInvalidatesCachedInternetGuardConfirmation() {
        ComputerControlInterface client(Computer({},QStringLiteral("Test PC"),QStringLiteral("192.0.2.25"),{},{}));
        client.setNetworkPolicyStatus(QStringLiteral("unblocked"));client.setProperty("schoolPolicyVerifiedState",QStringLiteral("allowed"));
        client.setProperty("schoolPolicyRequest",QStringLiteral("previous-session"));client.setProperty("schoolPolicyDeadline",12345);
        client.stop();QCOMPARE(client.property("schoolPolicyVerifiedState").toString(),QStringLiteral("unknown"));
        QVERIFY(client.property("schoolPolicyRequest").toString().isEmpty());QCOMPARE(client.property("schoolPolicyDeadline").toInt(),0);
        QVERIFY(client.networkPolicyStatus()!=QStringLiteral("unblocked"));
        client.setNetworkPolicyStatus(QStringLiteral("unblocked"));client.updateState();QVERIFY(client.networkPolicyStatus()!=QStringLiteral("unblocked"));
    }
    void ipv6DestinationsPreserveScopeAndDoNotConfuseHostWithPort() {
        struct Case {QString input,host;int port;};
        const QList<Case> cases{
            {"client.example:11100","client.example",11100},{"192.0.2.25","192.0.2.25",-1},
            {"192.0.2.25:11100","192.0.2.25",11100},{"2001:db8::25","2001:db8::25",-1},
            {"2001:db8::1234","2001:db8::1234",-1},{"[2001:db8::25]:11100","2001:db8::25",11100},
            {"fe80::25%17","fe80::25%17",-1},{"[fe80::25%17]","fe80::25%17",-1},{"[fe80::25%17]:11100","fe80::25%17",11100},
            {"::1","::1",-1},{"::1:11100","::1",11100},
            {"[::ffff:192.0.2.25]:11100","::ffff:192.0.2.25",11100}};
        for(const auto& value:cases) {
            QCOMPARE(HostAddress::parseHost(value.input),value.host);QCOMPARE(HostAddress::parsePortNumber(value.input),value.port);
            VncConnection connection(m_configuration);connection.setHost(value.input);
            QCOMPARE(connection.host(),value.host.startsWith("::ffff:")?QString("192.0.2.25"):value.host);
            QCOMPARE(connection.m_port,value.port);
        }
    }
    void stoppedDeletionIsIdempotentAndNeedsNoCoreLifetime()
    {
        auto connection=new VncConnection(m_configuration);
        QPointer<VncConnection> pointer(connection);
        connection->stopAndDeleteLater();
        connection->stopAndDeleteLater();
        QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        QVERIFY(pointer.isNull());
    }

    void rejectsDisconnectedAndStoppedEventsWithoutLeaking()
    {
        VncConnection connection(m_configuration);
        int destroyed=0,fired=0;
        connection.enqueueEvent(new CountingEvent(destroyed,fired));
        QCOMPARE(destroyed,1);QVERIFY(connection.isEventQueueEmpty());
        connection.m_state=VncConnection::State::Connected;
        connection.enqueueEvent(new CountingEvent(destroyed,fired));
        QVERIFY(!connection.isEventQueueEmpty());
        connection.stop();QCOMPARE(destroyed,2);QVERIFY(connection.isEventQueueEmpty());
        connection.enqueueEvent(new CountingEvent(destroyed,fired));
        QCOMPARE(destroyed,3);QCOMPARE(fired,0);
    }

    void boundsOutgoingQueueAndForcesReconnect()
    {
        VncConnection connection(m_configuration);
        connection.m_state=VncConnection::State::Connected;
        int destroyed=0,fired=0;
        for(int i=0;i<VncConnection::MaximumQueuedEvents+32;++i)
            connection.enqueueEvent(new CountingEvent(destroyed,fired));
        QCOMPARE(connection.m_eventQueue.size(),VncConnection::MaximumQueuedEvents);
        QVERIFY(connection.isControlFlagSet(VncConnection::ControlFlag::RestartConnection));
        QCOMPARE(destroyed,32);QCOMPARE(fired,0);
        connection.stop();QVERIFY(connection.isEventQueueEmpty());
        QCOMPARE(destroyed,VncConnection::MaximumQueuedEvents+32);
    }

    void limitsOutgoingBatchAndDiscardsStaleCommands()
    {
        VncConnection connection(m_configuration);
        connection.m_state=VncConnection::State::Connected;
        int destroyed=0,fired=0;
        for(int i=0;i<150;++i) connection.enqueueEvent(new CountingEvent(destroyed,fired));
        connection.sendEvents();QCOMPARE(fired,64);QCOMPARE(destroyed,64);
        QCOMPARE(connection.m_eventQueue.size(),86);
        connection.closeConnection();QVERIFY(connection.isEventQueueEmpty());
        QCOMPARE(destroyed,150);QCOMPARE(fired,64);
    }

    void publishesDetachedFramebufferAndPreservesRetainedSnapshots()
    {
        VncConnection connection(m_configuration);
        rfbClient client{};client.width=64;client.height=32;client.format.bitsPerPixel=32;
        connection.m_client=&client;
        QVERIFY(connection.initFrameBuffer());
        auto pixels=reinterpret_cast<quint32*>(client.frameBuffer);
        pixels[0]=qRgb(10,20,30);connection.finishFrameBufferUpdate();
        const auto retained=connection.image();
        QVERIFY(retained.constBits()!=client.frameBuffer);
        QCOMPARE(retained.pixel(0,0),qRgb(10,20,30));
        pixels[0]=qRgb(40,50,60);
        QCOMPARE(connection.image().pixel(0,0),qRgb(10,20,30));
        VncConnection::hookUpdateFB(&client,0,0,1,1);
        connection.m_pendingImageUpdate=QRect(0,0,1,1);
        connection.finishFrameBufferUpdate();
        QCOMPARE(connection.image().pixel(0,0),qRgb(40,50,60));
        QCOMPARE(retained.pixel(0,0),qRgb(10,20,30));
        client.width=32;client.height=16;QVERIFY(connection.initFrameBuffer());
        QCOMPARE(connection.image().size(),QSize(32,16));
        QCOMPARE(retained.pixel(0,0),qRgb(10,20,30));
        connection.m_client=nullptr;
    }

    void concurrentReadersSeeOnlyCompleteSnapshots()
    {
        VncConnection connection(m_configuration);
        rfbClient client{};client.width=128;client.height=64;client.format.bitsPerPixel=32;
        connection.m_client=&client;QVERIFY(connection.initFrameBuffer());
        std::atomic_bool finished{false};
        std::thread producer([&]() {
            auto pixels=reinterpret_cast<quint32*>(client.frameBuffer);
            for(int frame=1;frame<=1000;++frame) {
                std::fill_n(pixels,client.width*client.height,qRgb(frame&255,(frame>>2)&255,frame&255));
                connection.m_pendingImageUpdate=QRect(0,0,client.width,client.height);
                connection.finishFrameBufferUpdate();
            }
            finished.store(true);
        });
        bool mismatch=false;int reads=0;
        do {
            const auto snapshot=connection.image();
            const auto first=snapshot.pixel(0,0);
            for(int row=0;row<snapshot.height();++row)
                if(snapshot.pixel(snapshot.width()-1,row)!=first) mismatch=true;
            ++reads;
        } while(!finished.load());
        producer.join();QVERIFY(!mismatch);QVERIFY(reads>0);
        connection.m_client=nullptr;
    }

    void rejectsInvalidRemoteDimensionsBeforeAllocation_data()
    {
        QTest::addColumn<int>("width");QTest::addColumn<int>("height");
        QTest::newRow("zero")<<0<<1080;
        QTest::newRow("negative")<<1920<<-1;
        QTest::newRow("axis-limit")<<65535<<2;
        QTest::newRow("byte-limit")<<16384<<16384;
    }

    void rejectsInvalidRemoteDimensionsBeforeAllocation()
    {
        QFETCH(int,width);QFETCH(int,height);
        VncConnection connection(m_configuration);
        rfbClient client{};client.width=width;client.height=height;client.format.bitsPerPixel=32;
        connection.m_client=&client;
        QVERIFY(!connection.initFrameBuffer());QVERIFY(client.frameBuffer==nullptr);
        QVERIFY(connection.image().isNull());connection.m_client=nullptr;
    }

    void boundsDangerousConfiguredTimeouts()
    {
        m_configuration.setUseCustomVncConnectionSettings(true);
        m_configuration.setVncConnectionReadTimeout(0);
        m_configuration.setVncConnectionConnectTimeout(-1);
        m_configuration.setVncConnectionRetryInterval(-1);
        VncConnection connection(m_configuration);
        QCOMPARE(connection.m_readTimeout,1000);
        QCOMPARE(connection.m_connectTimeout,1000);
        QCOMPARE(connection.m_connectionRetryInterval,100);
        m_configuration.setUseCustomVncConnectionSettings(false);
    }

    void emptyUpdatesDoNotCopyOrRepaintFramebuffer()
    {
        VncConnection connection(m_configuration);
        rfbClient client{};client.width=64;client.height=32;client.format.bitsPerPixel=32;
        connection.m_client=&client;QVERIFY(connection.initFrameBuffer());
        connection.finishFrameBufferUpdate();
        const auto before=connection.image();
        QSignalSpy updated(&connection,&VncConnection::imageUpdated);
        QSignalSpy completed(&connection,&VncConnection::framebufferUpdateComplete);
        for(int i=0;i<100;++i) connection.finishFrameBufferUpdate();
        QCOMPARE(connection.image().constBits(),before.constBits());
        QCOMPARE(updated.count(),0);QCOMPARE(completed.count(),100);
        connection.m_client=nullptr;
    }

    void consoleHeartbeatMaintainsWatchdogWithoutRepaint()
    {
        VncConnection transport(m_configuration);
        ComputerControlInterface control(Computer{});
        control.m_state=VncConnection::State::Connected;
        control.connectFramebufferSignals(&transport);
        QSignalSpy repainted(&control,&ComputerControlInterface::framebufferUpdated);
        for(int i=0;i<100;++i) Q_EMIT transport.framebufferUpdateComplete();
        QCOMPARE(repainted.count(),0);
        QVERIFY(control.m_pingTimer.isActive());
        QVERIFY(control.m_connectionWatchdogTimer.isActive());
        Q_EMIT transport.imageUpdated(0,0,1,1);
        QCOMPARE(repainted.count(),1);
        control.stop();
        Q_EMIT transport.framebufferUpdateComplete();
        Q_EMIT transport.imageUpdated(0,0,1,1);
        QCOMPARE(repainted.count(),1);
        QVERIFY(!control.m_pingTimer.isActive());
        QVERIFY(!control.m_connectionWatchdogTimer.isActive());
    }

    void stopCannotLoseWakeupWhileWorkerEntersWait()
    {
        VncConnection connection(m_configuration);
        std::atomic_bool enteringWait{false};bool stopped=false;
        std::thread waiter([&]() {
            QMutexLocker lock(&connection.m_waitMutex);
            enteringWait.store(true);
            // Hold the mutex across the historical check-to-wait window.
            QThread::msleep(30);
            if (!connection.isControlFlagSet(VncConnection::ControlFlag::TerminateThread))
                connection.m_updateIntervalSleeper.wait(&connection.m_waitMutex,1000);
            stopped=connection.isControlFlagSet(VncConnection::ControlFlag::TerminateThread);
        });
        while(!enteringWait.load()) QThread::yieldCurrentThread();
        QElapsedTimer elapsed;elapsed.start();connection.stop();waiter.join();
        QVERIFY(stopped);QVERIFY(elapsed.elapsed()<900);
    }

    void recordsFullHdSnapshotPublicationCost()
    {
        VncConnection connection(m_configuration);
        rfbClient client{};client.width=1920;client.height=1080;client.format.bitsPerPixel=32;
        connection.m_client=&client;QVERIFY(connection.initFrameBuffer());
        QVector<qint64> samples;samples.reserve(50);
        for(int frame=1;frame<=50;++frame) {
            reinterpret_cast<quint32*>(client.frameBuffer)[0]=qRgb(frame,frame,frame);
            connection.m_pendingImageUpdate=QRect(0,0,client.width,client.height);
            QElapsedTimer elapsed;elapsed.start();connection.finishFrameBufferUpdate();
            samples.append(elapsed.nsecsElapsed());
            QCOMPARE(connection.image().pixel(0,0),qRgb(frame,frame,frame));
        }
        std::sort(samples.begin(),samples.end());
        const double averageNs=std::accumulate(samples.begin(),samples.end(),qint64(0))/double(samples.size());
        const double copiedMiB=client.width*client.height*4.0/(1024*1024);
        qInfo().nospace()<<"Full-HD immutable snapshot publication: 50 frames; average "
            <<averageNs/1e6<<" ms, p95 "<<samples.at(47)/1e6<<" ms, "
            <<copiedMiB/(averageNs/1e9)<<" MiB/s (informational, no hardware-specific threshold)";
        connection.m_client=nullptr;
    }
};

QTEST_GUILESS_MAIN(ConnectionReliabilityTests)
#include "ConnectionReliabilityTests.moc"
