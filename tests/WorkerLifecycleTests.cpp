// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025-26 prof. Ing. Raffaele Mele
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include <QtTest>
#include "FeatureWorkerManager.h"
#include "VeyonServerInterface.h"

class WorkerLifecycleTests : public QObject {
    Q_OBJECT
    class Server : public VeyonServerInterface {
    public:
        Server():VeyonServerInterface(nullptr) {}
        FeatureWorkerManager* manager=nullptr;
        FeatureWorkerManager& featureWorkerManager() override {return *manager;}
        bool sendFeatureMessageReply(const MessageContext&,const FeatureMessage&) override {return true;}
        int vncServerBasePort() const override {return 0;}
        void setMinimumFramebufferUpdateInterval(const MessageContext&,int) override {}
    };
private Q_SLOTS:
    void occupiedLoopbackRecoversWithoutRestartOrSpawningWorkers() {
        Server server;FeatureWorkerManager manager(server,nullptr,false);server.manager=&manager;
        QTcpServer holder;QVERIFY(holder.listen(QHostAddress::LocalHost,0));const auto port=holder.serverPort();
        QSignalSpy restored(&manager,&FeatureWorkerManager::listenerRestored);manager.initializeListener(port);
        QVERIFY(!manager.listenerReady());QVERIFY(manager.m_listenRetry.isActive());
        const auto uid=QUuid::createUuid();QVERIFY(!manager.startManagedSystemWorker(uid));
        QVERIFY(!manager.startUnmanagedSessionWorker(uid));manager.sendMessageToUnmanagedSessionWorker(FeatureMessage(uid,1));
        QVERIFY(manager.m_workers.isEmpty());QVERIFY(manager.m_retryGenerations.isEmpty());
        holder.close();QTRY_VERIFY_WITH_TIMEOUT(manager.listenerReady(),5000);
        QCOMPARE(manager.m_tcpServer.serverPort(),port);QCOMPARE(manager.m_tcpServer.serverAddress(),QHostAddress(QHostAddress::LocalHost));
        QCOMPARE(restored.count(),1);QVERIFY(!manager.m_listenRetry.isActive());QVERIFY(manager.m_workers.isEmpty());
    }
    void invalidPortCannotWrapOrRequestAnEphemeralListener() {
        Server server;FeatureWorkerManager manager(server,nullptr,false);
        for(qint64 bad:{-1,0,65536}) {
            manager.initializeListener(bad);QVERIFY(!manager.listenerReady());QVERIFY(!manager.m_listenRetry.isActive());
        }
    }
    void launchWithoutAuthenticatedIpcCannotRemainRunningForever() {
        Server server;FeatureWorkerManager manager(server,nullptr,false);server.manager=&manager;const auto uid=QUuid::createUuid();
        FeatureWorkerManager::Worker worker;worker.token="first-attempt";manager.m_workers.insert(uid,worker);
        manager.sendMessage(FeatureMessage(uid,1));QVERIFY(manager.isWorkerRunning(uid));manager.expireStartup(uid,"first-attempt");QVERIFY(!manager.isWorkerRunning(uid));
    }
    void oldDeadlineCannotRemoveReplacementWorker() {
        Server server;FeatureWorkerManager manager(server,nullptr,false);const auto uid=QUuid::createUuid();FeatureWorkerManager::Worker worker;worker.token="replacement";manager.m_workers.insert(uid,worker);
        manager.expireStartup(uid,"old-attempt");QVERIFY(manager.isWorkerRunning(uid));QCOMPARE(manager.m_workers[uid].token,QString("replacement"));
    }
    void authenticatedWorkerSurvivesStartupDeadlineAndDisconnectIsClean() {
        Server server;FeatureWorkerManager manager(server,nullptr,false);const auto uid=QUuid::createUuid();auto socket=new QTcpSocket(&manager);FeatureWorkerManager::Worker worker;worker.socket=socket;manager.m_workers.insert(uid,worker);
        manager.expireStartup(uid,"original-token");QVERIFY(manager.isWorkerRunning(uid));manager.closeConnection(socket);QVERIFY(!manager.isWorkerRunning(uid));
    }
    void startupQueueIsBoundedAndStopIsIdempotent() {
        Server server;FeatureWorkerManager manager(server,nullptr,false);const auto uid=QUuid::createUuid();FeatureWorkerManager::Worker worker;worker.token="fixture";manager.m_workers.insert(uid,worker);
        for(int i=0;i<140;++i) manager.sendMessage(FeatureMessage(uid,1));
        QCOMPARE(manager.m_workers[uid].pendingMessages.size(),128);QVERIFY(manager.stopWorker(uid));QVERIFY(!manager.stopWorker(uid));manager.expireStartup(uid,"fixture");QVERIFY(!manager.isWorkerRunning(uid));
    }
};
QTEST_GUILESS_MAIN(WorkerLifecycleTests)
#include "WorkerLifecycleTests.moc"
