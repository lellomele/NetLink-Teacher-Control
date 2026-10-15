// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025-26 prof. Ing. Raffaele Mele
#include "GuiLatencyMonitor.h"
#include <QDebug>

GuiLatencyMonitor::GuiLatencyMonitor(QObject* parent) : QObject(parent), m_timer(this)
{
    m_clock.start();
    m_timer.setInterval(250);
    m_timer.setTimerType(Qt::CoarseTimer);
    connect(&m_timer, &QTimer::timeout, this, [this] { sample(m_clock.elapsed()); });
    m_timer.start();
}

void GuiLatencyMonitor::sample(qint64 now)
{
    const auto previous = m_previous;
    m_previous = now;
    if(previous < 0 || now < previous) return;
    const auto delay = qMax<qint64>(0, now - previous - 250);
    m_maximumDelay = qMax(m_maximumDelay, delay);
    if(delay < 2000 || now - m_lastWarning < 30000) return;
    m_lastWarning = now;
    // Logged only after recovery: suspension/resume, OS scheduling and graphics
    // resets can also cause this gap. Never label it a proven deadlock.
    qWarning().noquote() << QStringLiteral("NETLINK_DIAG gui_event_loop_delay_ms=%1 recovered=true cause=unconfirmed (includes system sleep/resume)").arg(delay);
    Q_EMIT delayObserved(delay);
}
