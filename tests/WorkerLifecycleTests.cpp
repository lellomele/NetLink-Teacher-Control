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
