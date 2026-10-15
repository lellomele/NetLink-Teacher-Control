// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025-26 prof. Ing. Raffaele Mele
#pragma once
#include "VeyonCore.h"
#include <QAbstractNativeEventFilter>
#include <QColor>
#include <QObject>
#include <QPalette>

class QSettings;
class QWidget;

class VEYON_CORE_EXPORT NetLinkAppearance : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
public:
    struct Preferences {
        QString mode = QStringLiteral("System");
        QString preset = QStringLiteral("Oceano");
        QColor accent = QColor(QStringLiteral("#2563eb"));
    };
    static NetLinkAppearance* install(QSettings* settings = nullptr);
    ~NetLinkAppearance() override;
    static Preferences load(QSettings& settings);
    static bool save(QSettings& settings, const Preferences& preferences);
    static Preferences normalized(Preferences preferences);
    static QColor presetColor(const QString& preset);
    static QColor contrastingText(const QColor& background);
    static QPalette makePalette(const Preferences& preferences, bool systemDark);
    void showSettings(QWidget* parent);

Q_SIGNALS:
    void changed();

protected:
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    bool nativeEventFilter(const QByteArray&, void* message, qintptr*) override;
#else
    bool nativeEventFilter(const QByteArray&, void* message, long*) override;
#endif

private:
    explicit NetLinkAppearance(QObject* parent, QSettings* settings);
    void apply();
    Preferences m_preferences;
    QPalette m_originalPalette;
    QString m_originalStyle;
    bool m_queued = false;
    bool m_dialogOpen = false;
    bool m_highContrast = false;
    bool m_darkSystem = false;
    QSettings* m_settings;
};
