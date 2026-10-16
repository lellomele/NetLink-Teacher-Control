// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#include "LockActivity.h"
#include "NetLinkUi.h"
#include <QStringList>

namespace {
const QMap<QString,QString>& descriptions()
{
    static const QMap<QString,QString> values{
        {QStringLiteral("alt-tab"),QStringLiteral("Alt+Tab detected during NetLink lock")},
        {QStringLiteral("alt-esc"),QStringLiteral("Alt+Esc detected during NetLink lock")},
        {QStringLiteral("alt-space"),QStringLiteral("Alt+Space detected during NetLink lock")},
        {QStringLiteral("alt-f4"),QStringLiteral("Alt+F4 detected during NetLink lock")},
        {QStringLiteral("ctrl-esc"),QStringLiteral("Ctrl+Esc detected during NetLink lock")},
        {QStringLiteral("ctrl-shift-esc"),QStringLiteral("Ctrl+Shift+Esc detected during NetLink lock")},
        {QStringLiteral("hook-unavailable"),QStringLiteral("Ordinary shortcut filter could not be installed; keyboard monitoring is unavailable")},
        {QStringLiteral("windows-key"),QStringLiteral("Windows key detected during NetLink lock (combination unknown)")},
        {QStringLiteral("close-window"),QStringLiteral("Window-close request ignored during NetLink lock")},
        {QStringLiteral("session-locked"),QStringLiteral("Windows session locked; the triggering keys are unknown")},
        {QStringLiteral("session-unlocked"),QStringLiteral("Windows session unlocked; NetLink lock is still requested")},
        {QStringLiteral("secure-desktop"),QStringLiteral("Windows protected screen active (Ctrl+Alt+Del, UAC or another cause; exact cause unknown)")},
        {QStringLiteral("other-desktop"),QStringLiteral("Input moved away from the NetLink desktop; cause unknown")},
        {QStringLiteral("desktop-unavailable"),QStringLiteral("Input desktop cannot be inspected; this does not prove a shortcut was used")},
        {QStringLiteral("desktop-returned"),QStringLiteral("Returned to the ordinary desktop; NetLink cover restored")},
        {QStringLiteral("monitor-unavailable"),QStringLiteral("Windows session notifications unavailable; desktop checks remain active")}
    };
    return values;
}
}
bool LockActivity::validCode(const QString& code) {return descriptions().contains(code);}
bool LockActivity::validBatch(const QVariantMap& batch)
{
    if(batch.isEmpty() || batch.size()>descriptions().size()) return false;
    for(auto i=batch.cbegin();i!=batch.cend();++i)
        if(!validCode(i.key()) || i.value().userType()!=QMetaType::Int || i.value().toInt()<1 || i.value().toInt()>MaximumCount) return false;
    return true;
}
QString LockActivity::description(const QString& code) {return NetLinkUi::text(descriptions().value(code));}
void LockActivity::record(const QString& code)
{
    if(validCode(code)) m_pending[code]=qMin(MaximumCount,m_pending.value(code).toInt()+1);
}
QVariantMap LockActivity::take() {QVariantMap result;result.swap(m_pending);return result;}
