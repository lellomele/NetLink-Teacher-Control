// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025-26 prof. Ing. Raffaele Mele
#pragma once
#include "VeyonCore.h"
#include <QElapsedTimer>
#include <QTimer>

// Observes recovered event-loop delays; it is not a deadlock detector or a
// recovery mechanism. No thread, native hook, remote access or periodic I/O.
class VEYON_CORE_EXPORT GuiLatencyMonitor : public QObject
{
    Q_OBJECT
public:
    explicit GuiLatencyMonitor(QObject* parent = nullptr);
    qint64 maximumDelayMs() const { return m_maximumDelay; }
Q_SIGNALS:
    void delayObserved(qint64 milliseconds);
private:
    friend class GuiLatencyMonitorTests;
    void sample(qint64 now);
    QElapsedTimer m_clock;
    QTimer m_timer;
    qint64 m_previous = -1;
    qint64 m_lastWarning = -30000;
    qint64 m_maximumDelay = 0;
};
