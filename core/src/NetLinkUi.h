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
class QDialog;

namespace NetLinkUi {
VEYON_CORE_EXPORT QString copyrightText();
VEYON_CORE_EXPORT QString applicationName(const QString& configuredName);
// Product version for UI only; preserve the underlying Veyon protocol version.
VEYON_CORE_EXPORT QString displayVersion(const QString& technicalVersion);
// Shared UI catalog: canonical English, Italian fallback, then Qt translations.
VEYON_CORE_EXPORT QString text(const QString& source);
VEYON_CORE_EXPORT QString text(const char* source);
VEYON_CORE_EXPORT void installStandardTranslations();
// Native titles contain operational context only; attribution belongs in About.
VEYON_CORE_EXPORT QString windowTitle(const QString& title);
// Reuse one About window per owner; closing it must not terminate a tray worker.
VEYON_CORE_EXPORT QDialog* showAbout(QWidget* parent = nullptr);
// Original, resolution-independent QPainter paths; no icon font/SVG runtime.
VEYON_CORE_EXPORT QIcon icon(const QString& name, const QString& set, const QColor& accent,
                            const QIcon& fallback = {});
VEYON_CORE_EXPORT QString featureDescription(const Feature& feature);
VEYON_CORE_EXPORT void addInformationMenu(QMainWindow* window);
VEYON_CORE_EXPORT bool easyPage(const QString& className);
VEYON_CORE_EXPORT void setEasyPage(QWidget* page, bool easy);
VEYON_CORE_EXPORT void setPropertyVisibility(QWidget* widget, bool advanced);
// Pure formatting: no DNS, sockets, service queries or changes to the machine.
VEYON_CORE_EXPORT QString traySummary(const QString& host, int port, bool listening,
    bool localOnly, bool keyAuthentication, bool discovery, int authenticated, int pending);
struct ServiceSnapshot {
    QString host, bindAddress, plugin;
    QStringList localAddresses;
    int port=0, backendPort=0, authorized=0, pending=0;
    bool listening=false, external=false, discovery=false;
    bool backendChecked=false, backendAvailable=false, backendReady=false;
    qint64 frameAgeMs=-1;
};
VEYON_CORE_EXPORT QString serviceSnapshot(const ServiceSnapshot& status);
VEYON_CORE_EXPORT QMessageBox* showTrayDetails(const QString& snapshot,const QString& version,const QString& updated);
}
