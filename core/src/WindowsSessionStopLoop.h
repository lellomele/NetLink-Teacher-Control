// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "WindowsSessionStop.h"
#include <QEventLoop>
#include <QWinEventNotifier>

namespace WindowsSessionStop {
inline bool waitInEventLoop(const QString& name) {
    Event event;
    if(!event.open(name,SYNCHRONIZE)) return false;
    QEventLoop loop;
    QWinEventNotifier notifier(event.handle());
    QObject::connect(&notifier,&QWinEventNotifier::activated,&loop,[&]{notifier.setEnabled(false);loop.quit();});
    loop.exec();
    return true;
}
}
