// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#pragma once
#include "VeyonCore.h"
#include <QUuid>
#include <QVector>

// Local Master document. Never sent through discovery or to the clients.
struct VEYON_CORE_EXPORT SchoolClassProfile {
    struct Assignment { QUuid computerId; QString computerName, hostHint, student; };
    QString className, room;
    QVector<Assignment> assignments;
    static constexpr int MaximumAssignments = 256;
    static constexpr int MaximumBytes = 1024 * 1024;
    static bool validText(const QString& text, int maximum, bool allowEmpty = true);
    bool validate(QString* error = nullptr) const;
    QByteArray toJson() const;
    static bool fromJson(const QByteArray&, SchoolClassProfile&, QString* error = nullptr);
    static bool load(const QString&, SchoolClassProfile&, QString* error = nullptr);
    bool save(const QString&, QString* error = nullptr) const;
    static QString defaultDirectory();
};
