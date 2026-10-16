// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include <QtTest>
#include <QBuffer>
#include <QDialog>
#include <QMainWindow>
#include <QPlainTextEdit>
#include "LockWidget.h"
#include "ScreenLockFeaturePlugin.h"
#include "VeyonServerInterface.h"
#include "VeyonWorkerInterface.h"
#ifdef Q_OS_WIN
#include <windows.h>
#include "WindowsKeyboardShortcutTrapper.h"
#endif

class LockTestServer : public VeyonServerInterface {
public:
    LockTestServer():VeyonServerInterface(nullptr) {}
    FeatureWorkerManager& featureWorkerManager() override {qFatal("No runtime worker may be started by this test");}
    bool sendFeatureMessageReply(const MessageContext& context,const FeatureMessage& message) override {target=context.ioDevice();replies.append(message);return true;}
    int vncServerBasePort() const override {return 11100;}
    void setMinimumFramebufferUpdateInterval(const MessageContext&,int) override {}
    QIODevice* target=nullptr;
    QList<FeatureMessage> replies;
};
class LockTestWorker : public VeyonWorkerInterface {
public:
    bool sendFeatureMessageReply(const FeatureMessage& reply) override {replies.append(reply);return true;}
    QList<FeatureMessage> replies;
};

class LockNotificationTests : public QObject {
    Q_OBJECT
    QLocale m_initialLocale;
private Q_SLOTS:
    void initTestCase() {QLocale::setDefault(QLocale("en_US"));}
    void cleanupTestCase() {QLocale::setDefault(m_initialLocale);}
#ifdef Q_OS_WIN
    void ordinaryShortcutClassificationNeverPretendsToObserveSAS() {
        using T=WindowsKeyboardShortcutTrapper;
        QCOMPARE(T::classifyShortcut(VK_TAB,false,false,true),T::AltTab);
        QCOMPARE(T::classifyShortcut(VK_F4,false,false,true),T::AltF4);
        QCOMPARE(T::classifyShortcut(VK_ESCAPE,true,false,false),T::CtrlEsc);
        QCOMPARE(T::classifyShortcut(VK_ESCAPE,true,true,false),T::CtrlShiftEsc);
        QCOMPARE(T::classifyShortcut(VK_LWIN,false,false,false),T::SuperKeyDown);
        QCOMPARE(T::classifyShortcut(VK_DELETE,true,false,true),T::NoShortcut);
        QCOMPARE(T::classifyShortcut('A',false,false,false),T::NoShortcut);
        QCOMPARE(T::classifyShortcut('L',false,false,false),T::NoShortcut); // Win+L is inferred from no keys
    }
#endif
    void boundedAllowlistNeverContainsTypedText() {
        LockActivity events;events.record("typed-password");QVERIFY(events.take().isEmpty());
        for(int i=0;i<20000;++i) events.record("alt-tab");
        events.record("secure-desktop");const auto batch=events.take();
        QVERIFY(LockActivity::validBatch(batch));QCOMPARE(batch.size(),2);
        QCOMPARE(batch["alt-tab"].toInt(),LockActivity::MaximumCount);QVERIFY(events.take().isEmpty());
        QVERIFY(!LockActivity::validBatch({{"arbitrary-text",1}}));
        QVERIFY(!LockActivity::validBatch({{"alt-tab","1"}}));
        QVERIFY(!LockActivity::validBatch({{"alt-tab",0}}));
        QVERIFY(!LockActivity::validBatch({{"alt-tab",10000}}));
    }
    void desktopChangesAreDistinctFromKeysAndDeduplicated() {
        LockWidget cover(LockWidget::BackgroundPixmap,{},nullptr,false);
        QSignalSpy observed(&cover,&LockWidget::activityObserved);
        cover.observeDesktopState(0);QCOMPARE(observed.size(),0);
        cover.observeDesktopState(1);cover.observeDesktopState(1);QCOMPARE(observed.size(),1);
        QCOMPARE(observed.takeFirst().first().toString(),QString("secure-desktop"));
        cover.hide();cover.observeDesktopState(0);QVERIFY(cover.isVisible());
        QCOMPARE(observed.takeFirst().first().toString(),QString("desktop-returned"));
        cover.observeDesktopState(3);QCOMPARE(observed.takeFirst().first().toString(),QString("desktop-unavailable"));
        QVERIFY(!cover.close());QCOMPARE(observed.takeFirst().first().toString(),QString("close-window"));
        QVERIFY(LockActivity::description("secure-desktop").contains("unknown"));
        QVERIFY(LockActivity::description("desktop-unavailable").contains("does not prove"));
    }
    void workerAggregatesWithoutPollingOrSendingKeystrokes() {
        ScreenLockFeaturePlugin plugin;LockTestWorker worker;
        plugin.m_workerFeature=plugin.m_screenLockFeature.uid();
        plugin.m_workerToken=QUuid::createUuid().toString(QUuid::WithoutBraces);
        plugin.m_activity.record("alt-tab");plugin.m_activity.record("alt-tab");
        plugin.flushActivity(worker);plugin.flushActivity(worker);
        QCOMPARE(worker.replies.size(),1);const auto& message=worker.replies.first();
        QCOMPARE(message.argument(0).toString(),plugin.m_workerToken);
        QCOMPARE(message.argument(1).toULongLong(),quint64(1));
        QCOMPARE(message.argument(2).toMap().value("alt-tab").toInt(),2);
        QCOMPARE(message.arguments().size(),3);
        plugin.m_workerToken.clear();plugin.m_activity.record("alt-f4");plugin.flushActivity(worker);
        QCOMPARE(worker.replies.size(),1);
    }
    void serverOnlyRelaysAuthenticatedWorkerAndCurrentLock() {
        ScreenLockFeaturePlugin plugin;LockTestServer server;QBuffer remote,ipc;QObject remoteConnection;
        const auto token=QUuid::createUuid().toString(QUuid::WithoutBraces);
        plugin.m_monitorToken=token;plugin.m_monitorContext=MessageContext(&remote,&remoteConnection);
        plugin.m_monitoredFeature=plugin.m_screenLockFeature.uid();
        const auto event=[&](quint64 seq,const QString& id,const QVariantMap& batch) {
            auto result=QSharedPointer<FeatureMessage>::create(plugin.m_monitoredFeature,ScreenLockFeaturePlugin::LockEventsCommand);
            result->addArgument(0,id).addArgument(1,seq).addArgument(2,batch);return result;
        };
        const auto message(event(1,token,{{"secure-desktop",1}}));
        plugin.handleFeatureMessage(server,MessageContext(&ipc),*message);QVERIFY(server.replies.isEmpty());
        ipc.setProperty("workerAuthenticated",true);
        plugin.handleFeatureMessage(server,MessageContext(&ipc,&remoteConnection),*message);QVERIFY(server.replies.isEmpty());
        plugin.handleFeatureMessage(server,MessageContext(&ipc),*message);QCOMPARE(server.replies.size(),1);QCOMPARE(server.target,&remote);
        plugin.handleFeatureMessage(server,MessageContext(&ipc),*message);QCOMPARE(server.replies.size(),1);
        plugin.m_receiveLimit.invalidate();
        plugin.handleFeatureMessage(server,MessageContext(&ipc),*event(2,"other-lock",{{"alt-tab",1}}));
        plugin.handleFeatureMessage(server,MessageContext(&ipc),*event(2,token,{{"typed-text",1}}));QCOMPARE(server.replies.size(),1);
        plugin.handleFeatureMessage(server,MessageContext(&ipc),*event(2,token,{{"alt-tab",2}}));QCOMPARE(server.replies.size(),2);
        plugin.handleFeatureMessage(server,MessageContext(&ipc),*event(3,token,{{"alt-tab",2}}));QCOMPARE(server.replies.size(),2); // rate limit
        plugin.m_monitorToken.clear();plugin.m_receiveLimit.invalidate();
        plugin.handleFeatureMessage(server,MessageContext(&ipc),*event(3,token,{{"alt-tab",2}}));QCOMPARE(server.replies.size(),2);
    }
    void masterShowsSingleBoundedNonmodalPlainTextLogAndRejectsReplay() {
        ScreenLockFeaturePlugin plugin;QMainWindow main;plugin.m_masterWindow=&main;
        const auto client=ComputerControlInterface::Pointer::create(Computer({},"PC <b>01</b>","192.0.2.10"));
        const auto token=QUuid::createUuid().toString(QUuid::WithoutBraces);client->setProperty("netlinkLockToken",token);
        const auto event=[&](int seq,const QString& id=QString{}) {
            auto result=QSharedPointer<FeatureMessage>::create(plugin.m_screenLockFeature.uid(),ScreenLockFeaturePlugin::LockEventsCommand);
            result->addArgument(0,id.isEmpty()?token:id).addArgument(1,qulonglong(seq)).addArgument(2,QVariantMap{{"secure-desktop",1}});return result;
        };
        plugin.handleFeatureMessage(client,*event(1));QVERIFY(plugin.m_eventsWindow);QVERIFY(!plugin.m_eventsWindow->isModal());
        auto text=plugin.m_eventsText.data();QVERIFY(text->toPlainText().contains("PC <b>01</b>"));QVERIFY(text->toPlainText().contains("unknown"));
        plugin.handleFeatureMessage(client,*event(1));QCOMPARE(text->document()->blockCount(),1);
        plugin.handleFeatureMessage(client,*event(2,"wrong-token"));QCOMPARE(text->document()->blockCount(),1);
        for(int i=2;i<=260;++i) plugin.handleFeatureMessage(client,*event(i));
        QCOMPARE(text->document()->blockCount(),200);QCOMPARE(main.findChildren<QDialog*>("netlinkLockEvents").size(),1);
        client->setProperty("netlinkLockToken",QVariant{});plugin.handleFeatureMessage(client,*event(261));QCOMPARE(text->document()->blockCount(),200);
        delete plugin.m_eventsWindow;QVERIFY(!plugin.m_eventsWindow);QVERIFY(!plugin.m_eventsText);
    }
};
QTEST_MAIN(LockNotificationTests)
#include "LockNotificationTests.moc"
