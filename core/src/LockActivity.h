// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#pragma once
#include "VeyonCore.h"
#include <QVariantMap>

// Only whitelisted lock/session events, never ordinary keystrokes or text.
class VEYON_CORE_EXPORT LockActivity
{
public:
    static constexpr int MaximumCount=9999;
    static bool validCode(const QString& code);
    static bool validBatch(const QVariantMap& batch);
    static QString description(const QString& code);
    void record(const QString& code);
    QVariantMap take();
private:
    QVariantMap m_pending;
};
