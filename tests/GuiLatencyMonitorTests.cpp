// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025-26 prof. Ing. Raffaele Mele
#include "GuiLatencyMonitor.h"
#include <QtTest>

class GuiLatencyMonitorTests : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void normalTicksDoNotWarn() {
        GuiLatencyMonitor monitor;
        monitor.m_timer.stop();
        QSignalSpy observed(&monitor, &GuiLatencyMonitor::delayObserved);
        for(qint64 now = 0; now < 10000; now += 250) monitor.sample(now);
        QCOMPARE(observed.count(), 0);
        QCOMPARE(monitor.maximumDelayMs(), qint64(0));
    }
    void startupAndBackwardClockDoNotWarn() {
        GuiLatencyMonitor monitor; monitor.m_timer.stop();
        QSignalSpy observed(&monitor, &GuiLatencyMonitor::delayObserved);
        monitor.sample(100000); monitor.sample(10); monitor.sample(260);
        QCOMPARE(observed.count(), 0);
    }
    void recoveredDelayIsRateLimited() {
        GuiLatencyMonitor monitor; monitor.m_timer.stop();
        QSignalSpy observed(&monitor, &GuiLatencyMonitor::delayObserved);
        monitor.sample(0);
        QTest::ignoreMessage(QtWarningMsg, "NETLINK_DIAG gui_event_loop_delay_ms=2750 recovered=true cause=unconfirmed (includes system sleep/resume)");
        monitor.sample(3000); monitor.sample(6000); monitor.sample(9000);
        QCOMPARE(observed.count(), 1); QCOMPARE(observed[0][0].toLongLong(), qint64(2750));
        QTest::ignoreMessage(QtWarningMsg, "NETLINK_DIAG gui_event_loop_delay_ms=24750 recovered=true cause=unconfirmed (includes system sleep/resume)");
        monitor.sample(34000);
        QCOMPARE(observed.count(), 2); QCOMPARE(monitor.maximumDelayMs(), qint64(24750));
    }
    void destructionCancelsTimer() {
        QObject parent; auto monitor = new GuiLatencyMonitor(&parent);
        QPointer<GuiLatencyMonitor> guard(monitor); delete monitor;
        QVERIFY(guard.isNull()); QCoreApplication::processEvents();
    }
};
QTEST_GUILESS_MAIN(GuiLatencyMonitorTests)
#include "GuiLatencyMonitorTests.moc"
