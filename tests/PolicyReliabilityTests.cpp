// SPDX-License-Identifier: GPL-2.0-or-later
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include <QtTest>
#include <QJsonArray>
#include "SchoolNetworkPolicy.h"

// No named pipe listener and no native WFP function is opened or called. The
// private friend-only constructor replaces just EngineClose for failure tests.
class PolicyReliabilityTests : public QObject {
    Q_OBJECT
    static void seed(SchoolNetwork::PolicyDaemon& daemon) {
        daemon.m_engine=reinterpret_cast<void*>(quintptr(1));
        daemon.m_owner="4ba07bcc-e9c1-4300-94dc-10f53b55d54a";
        daemon.m_filters={11,12};daemon.m_digest="policy-fingerprint";daemon.m_lease.start();
    }
private Q_SLOTS:
    void failedCloseRetainsAllRecoveryStateAndReportsUnknown() {
        int calls=0;quint32 code=5;
        SchoolNetwork::PolicyDaemon daemon(nullptr,false,[&](void* engine) {Q_ASSERT(engine==reinterpret_cast<void*>(quintptr(1)));++calls;return code;});
        seed(daemon);const auto owner=daemon.m_owner;const auto digest=daemon.m_digest;
        QCOMPARE(daemon.release(),quint32(5));QCOMPARE(calls,1);QVERIFY(daemon.m_engine);QCOMPARE(daemon.m_owner,owner);QCOMPARE(daemon.m_filters,QVector<quint64>({11,12}));QCOMPARE(daemon.m_digest,digest);
        const auto status=daemon.apply({{"op","status"}});QVERIFY(!status.value("ok").toBool());QCOMPARE(status.value("status").toString(),QString("unknown"));QCOMPARE(status.value("code").toDouble(),5.0);QVERIFY(status.value("cleanupPending").toBool());QCOMPARE(calls,1);
        code=0;QCOMPARE(daemon.release(),quint32(0));QCOMPARE(calls,2);QVERIFY(!daemon.m_engine);QVERIFY(daemon.m_owner.isEmpty());QVERIFY(daemon.m_filters.isEmpty());QVERIFY(daemon.m_digest.isEmpty());QVERIFY(!daemon.m_lease.isValid());
        const auto released=daemon.apply({{"op","status"}});QVERIFY(released.value("ok").toBool());QCOMPARE(released.value("status").toString(),QString("allowed"));
    }
    void pendingCleanupCannotOpenOrRenewAPolicy() {
        int calls=0;quint32 code=1722;SchoolNetwork::PolicyDaemon daemon(nullptr,false,[&](void*) {++calls;return code;});seed(daemon);
        QCOMPARE(daemon.release(),quint32(1722));const auto owner=daemon.m_owner;
        const auto result=daemon.apply({{"op","apply"},{"owner",owner},{"addresses",QJsonArray{"192.0.2.25"}}});
        QVERIFY(!result.value("ok").toBool());QCOMPARE(result.value("status").toString(),QString("cleanup-pending"));QCOMPARE(result.value("code").toDouble(),1722.0);QCOMPARE(calls,1);QCOMPARE(daemon.m_owner,owner);
        code=0;daemon.release();
    }
    void automaticCleanupUsesBackoffAndExplicitReleaseCanRetry() {
        int calls=0;quint32 code=5;SchoolNetwork::PolicyDaemon daemon(nullptr,false,[&](void*) {++calls;return code;});seed(daemon);daemon.release();
        for(int i=0;i<1000;++i) {daemon.maintainLease();}
        QCOMPARE(calls,1);
        daemon.m_cleanupRetry.invalidate();daemon.maintainLease();QCOMPARE(calls,2);QCOMPARE(daemon.m_cleanupAttempts,2);
        for(int i=0;i<1000;++i) {daemon.maintainLease();}
        QCOMPARE(calls,2);
        const auto release=daemon.apply({{"op","release"},{"owner",daemon.m_owner}});QVERIFY(!release.value("ok").toBool());QCOMPARE(release.value("status").toString(),QString("unknown"));QCOMPARE(calls,3);
        code=0;daemon.release();
    }
    void anotherOwnerCannotForcePendingCleanup() {
        int calls=0;quint32 code=5;SchoolNetwork::PolicyDaemon daemon(nullptr,false,[&](void*) {++calls;return code;});seed(daemon);daemon.release();
        const auto result=daemon.apply({{"op","release"},{"owner","45b5cdb1-dbf4-426d-97b2-8798662b2884"}});QVERIFY(!result.value("ok").toBool());QCOMPARE(result.value("status").toString(),QString("owned-by-another-session"));QCOMPARE(calls,1);
        code=0;daemon.release();
    }
    void invalidHandleErrorDoesNotPretendFiltersDisappeared() {
        quint32 code=6;SchoolNetwork::PolicyDaemon daemon(nullptr,false,[&](void*) {return code;});seed(daemon);QCOMPARE(daemon.release(),quint32(6));QVERIFY(daemon.m_engine);
        const auto status=daemon.apply({{"op","status"}});QVERIFY(!status.value("ok").toBool());QCOMPARE(status.value("status").toString(),QString("unknown"));QCOMPARE(status.value("code").toDouble(),6.0);
        code=0;daemon.release();
    }
    void emptyDaemonIsIdempotentAndDoesNotCallNativeCleanup() {
        int calls=0;SchoolNetwork::PolicyDaemon daemon(nullptr,false,[&](void*) {++calls;return quint32(0);});
        QCOMPARE(daemon.release(),quint32(0));daemon.maintainLease();QCOMPARE(calls,0);
        const auto status=daemon.apply({{"op","status"}});QVERIFY(status.value("ok").toBool());QCOMPARE(status.value("status").toString(),QString("allowed"));
    }
};
QTEST_GUILESS_MAIN(PolicyReliabilityTests)
#include "PolicyReliabilityTests.moc"
