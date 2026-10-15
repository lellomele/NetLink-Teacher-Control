// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025-26 prof. Ing. Raffaele Mele
#pragma once
#include <QJsonObject>

namespace Companion {
// Bounded, read-only projection. Raw event XML, window titles and unrelated
// application events are never retained in a Companion report.
QJsonObject crashEvent(const QByteArray& xml);
QJsonObject collectHealth();
}
