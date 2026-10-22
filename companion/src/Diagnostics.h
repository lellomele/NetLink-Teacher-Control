// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025-26 prof. Ing. Raffaele Mele
#pragma once
#include <QJsonObject>
#include <QStringList>

namespace Companion {
constexpr int MaximumReportBytes = 2 * 1024 * 1024;
QJsonObject collect();
bool parseReport(const QByteArray& bytes, QJsonObject& report, QString& error);
QJsonObject publicReport(const QJsonObject& report, bool includeIdentifiers);
QJsonObject exportedReport(const QJsonObject& report, bool includeIdentifiers);
QStringList recommendations(const QJsonObject& report, const QString& role);
QStringList compare(const QJsonObject& teacher, const QJsonObject& client);
QString html(const QJsonObject& report, const QString& role);
bool validTarget(const QString& target);
}
