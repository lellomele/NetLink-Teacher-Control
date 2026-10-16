// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#pragma once
#include "VeyonCore.h"
#include <QColor>
#include <QIcon>
#include <QStringList>

class QMainWindow;
class QWidget;
class Feature;
class QMessageBox;

namespace NetLinkUi {
VEYON_CORE_EXPORT QString copyrightText();
// Native title-bar text: one suffix, without consuming the working area.
VEYON_CORE_EXPORT QString windowTitle(const QString& title);
// Original, resolution-independent QPainter paths; no icon font/SVG runtime.
VEYON_CORE_EXPORT QIcon icon(const QString& name, const QString& set, const QColor& accent,
                            const QIcon& fallback = {});
VEYON_CORE_EXPORT QString featureDescription(const Feature& feature);
VEYON_CORE_EXPORT void addInformationMenu(QMainWindow* window);
VEYON_CORE_EXPORT bool easyPage(const QString& className);
VEYON_CORE_EXPORT void setEasyPage(QWidget* page, bool easy);
// Pure formatting: no DNS, sockets, service queries or changes to the machine.
VEYON_CORE_EXPORT QString traySummary(const QString& host, int port, bool listening,
    bool localOnly, bool keyAuthentication, bool discovery, int authenticated, int pending);
VEYON_CORE_EXPORT QMessageBox* showTrayDetails(const QString& snapshot,const QString& version,const QString& updated);
}
