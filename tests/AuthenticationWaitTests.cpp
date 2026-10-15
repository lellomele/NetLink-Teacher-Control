// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025-26 prof. Ing. Raffaele Mele
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include <QtTest>
#include <QSemaphore>
#include <QTemporaryDir>
#include <QWaitCondition>
#include <QtCrypto>
#include <thread>
#include "AuthenticationProxy.h"
#include "Configuration/JsonStore.h"
#include "VeyonConfiguration.h"
#include "../plugins/webapi/WebApiAuthenticationProxy.h"
#include "../plugins/webapi/WebApiConfiguration.h"

IMPLEMENT_CONFIG_PROXY(WebApiConfiguration)

class AuthenticationWaitTests : public QObject
{
    Q_OBJECT
    QCA::Initializer m_cryptoInitializer;
    class Proxy : public AuthenticationProxy
    {
    public:
        RfbVeyonAuth::Type initCredentials() override {return RfbVeyonAuth::Invalid;}
        QMutex* sharedMutex() {return mutex();}
        template<class Predicate>
        bool controlledWait(QWaitCondition& condition, int timeout, Predicate ready)
        {return waitForCondition(condition,timeout,ready);}
    };
private Q_SLOTS:
    void reproducesLegacyLostWakeupWithoutSchedulingAssumptions()
    {
        QMutex publisherMutex;QWaitCondition available;
        QSemaphore checked,allowWait;
        bool ready=false,result=true;
        std::thread waiter([&]() {
            QMutex localWaitMutex;QMutexLocker localLock(&localWaitMutex);
            bool observed;
            {QMutexLocker lock(&publisherMutex);observed=ready;}
            checked.release();allowWait.acquire();
            // This is the old check/local-mutex/wait algorithm, with its exact
            // race window controlled by semaphores rather than thread sleeps.
            result=observed || available.wait(&localWaitMutex,20);
        });
        checked.acquire();
        {QMutexLocker lock(&publisherMutex);ready=true;}
        available.wakeAll();allowWait.release();waiter.join();
        QVERIFY(ready);QVERIFY(!result);
    }

    void sharedPredicateWaitCannotLosePublisherNotification()
    {
        Proxy proxy;QWaitCondition available;
        QSemaphore checked,allowPredicate,publisherAttempted;
        bool ready=false,result=false;int checks=0;
        std::thread waiter([&]() {
            result=proxy.controlledWait(available,3000,[&]() {
                if(++checks==1) {checked.release();allowPredicate.acquire();}
                return ready;
            });
        });
        checked.acquire();
        std::thread publisher([&]() {
            publisherAttempted.release();
            {QMutexLocker lock(proxy.sharedMutex());ready=true;}
            available.wakeAll();
        });
        publisherAttempted.acquire();allowPredicate.release();
        waiter.join();publisher.join();QVERIFY(result);QVERIFY(ready);QVERIFY(checks>=2);
    }

    void spuriousWakeDoesNotSatisfyPredicate()
    {
        Proxy proxy;QWaitCondition available;
        QSemaphore firstCheck,resume,secondCheck;
        bool ready=false,result=false;int checks=0;
        std::thread waiter([&]() {
            result=proxy.controlledWait(available,3000,[&]() {
                ++checks;
                if(checks==1) {firstCheck.release();resume.acquire();}
                if(checks==2) secondCheck.release();
                return ready;
            });
        });
        firstCheck.acquire();resume.release();
        // Acquiring the same mutex guarantees the waiter registered its wait
        // before we issue a deliberately empty update.
        {QMutexLocker lock(proxy.sharedMutex());available.wakeAll();}
        secondCheck.acquire();
        {QMutexLocker lock(proxy.sharedMutex());ready=true;available.wakeAll();}
        waiter.join();QVERIFY(result);QVERIFY(checks>=3);
    }

    void authenticationMethodsRequireActualReadiness()
    {
        Proxy proxy;QVERIFY(!proxy.waitForAuthenticationMethods(0));
        proxy.setAuthenticationTypes({RfbVeyonAuth::KeyFile});
        QVERIFY(proxy.waitForAuthenticationMethods(0));
        proxy.notifyProtocolError();
        QVERIFY(!proxy.waitForAuthenticationMethods(60000));
    }

    void pendingProtocolErrorNeverWaitsForTimeout()
    {
        Proxy proxy;proxy.notifyProtocolError();
        QElapsedTimer elapsed;elapsed.start();
        QVERIFY(!proxy.waitForAuthenticationMethods(5000));
        QVERIFY(elapsed.elapsed()<1000);
    }

    void realWebApiCredentialsAlreadyPublishedAreConsumed()
    {
        QTemporaryDir directory;QVERIFY(directory.isValid());
        Configuration::JsonStore store(Configuration::Store::System,directory.filePath("webapi.json"));
        VeyonConfiguration configuration(&store);WebApiConfiguration web(&configuration);
        WebApiAuthenticationProxy proxy(web);
        const QUuid method("63611f7c-b457-42c7-832e-67d0f9281085");
        QVERIFY(proxy.populateCredentials(method,{{"username","test-user"},{"password","synthetic-not-a-real-password"}}));
        QCOMPARE(proxy.initCredentials(),RfbVeyonAuth::Logon);
    }
};
QTEST_GUILESS_MAIN(AuthenticationWaitTests)
#include "AuthenticationWaitTests.moc"
