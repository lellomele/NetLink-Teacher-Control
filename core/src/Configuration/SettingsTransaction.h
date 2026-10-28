// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#pragma once
#include <QMap>
#include <QVariant>
#include <QString>
#include <memory>
#include "VeyonCore.h"
class QSettings;
namespace Configuration {
// Cooperating writers/readers serialize one physical settings store. No key files.
class VEYON_CORE_EXPORT SettingsLock {
public:
    explicit SettingsLock(const QSettings& settings);
    ~SettingsLock();
    bool locked() const;
    SettingsLock(const SettingsLock&)=delete;
    SettingsLock& operator=(const SettingsLock&)=delete;
private:
    struct State;std::unique_ptr<State> m_state;
};
namespace SettingsTransaction {
using Values=QMap<QString,QVariant>;
VEYON_CORE_EXPORT QString identity(const QSettings& settings);
VEYON_CORE_EXPORT bool read(QSettings& settings,Values& values,QString& error);
VEYON_CORE_EXPORT bool recover(QSettings& settings,QString& error);
VEYON_CORE_EXPORT bool commit(QSettings& settings,const Values& before,const Values& after,QString& error);
// Three-way merge. QJson arrays/objects are serialized single values, never silently merged.
VEYON_CORE_EXPORT bool merge(const Values& baseline,const Values& desired,const Values& current,Values& merged,QString& error);
VEYON_CORE_EXPORT bool reserved(const QString& key);
VEYON_CORE_EXPORT QByteArray recoveryRecord(const QSettings& settings,const Values& before,const Values& after);
}
}
