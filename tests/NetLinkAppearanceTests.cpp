// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025-26 prof. Ing. Raffaele Mele
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include <QtTest>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <cmath>
#include "NetLinkAppearance.h"

class NetLinkAppearanceTests : public QObject {
    Q_OBJECT
    static double luminance(const QColor& color) {
        const auto linear=[](double value){return value<=.04045?value/12.92:std::pow((value+.055)/1.055,2.4);};
        return .2126*linear(color.redF())+.7152*linear(color.greenF())+.0722*linear(color.blueF());
    }
private Q_SLOTS:
    void preferenceRoundTrip() {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        const auto path=directory.filePath("appearance.ini");
        QSettings settings(path,QSettings::IniFormat);
        NetLinkAppearance::Preferences preferences;preferences.mode="Dark";preferences.preset="Personalizzato";preferences.accent=QColor("#d3a011");
        settings.setValue("unrelated","preserved");
        QVERIFY(NetLinkAppearance::save(settings,preferences));
        QSettings reopened(path,QSettings::IniFormat);const auto loaded=NetLinkAppearance::load(reopened);
        QCOMPARE(loaded.mode,preferences.mode);QCOMPARE(loaded.preset,preferences.preset);QCOMPARE(loaded.accent,preferences.accent);
        QCOMPARE(reopened.value("unrelated").toString(),QString("preserved"));
    }
    void invalidPreferencesFallBack() {
        NetLinkAppearance::Preferences preferences;preferences.mode="invalid";preferences.preset="invalid";preferences.accent=QColor();
        preferences=NetLinkAppearance::normalized(preferences);
        QCOMPARE(preferences.mode,QString("System"));QCOMPARE(preferences.preset,QString("Oceano"));QCOMPARE(preferences.accent,QColor("#2563eb"));
        preferences.preset="Personalizzato";preferences.accent=QColor(10,40,80,0);preferences=NetLinkAppearance::normalized(preferences);
        QCOMPARE(preferences.accent.alpha(),255);
    }
    void paletteModes() {
        NetLinkAppearance::Preferences preferences;preferences.mode="Light";
        QVERIFY(luminance(NetLinkAppearance::makePalette(preferences,true).color(QPalette::Window))>.8);
        preferences.mode="Dark";QVERIFY(luminance(NetLinkAppearance::makePalette(preferences,false).color(QPalette::Window))<.03);
        preferences.mode="System";
        QVERIFY(luminance(NetLinkAppearance::makePalette(preferences,true).color(QPalette::Window))<.03);
        QVERIFY(luminance(NetLinkAppearance::makePalette(preferences,false).color(QPalette::Window))>.8);
        for(const auto& preset:{"Oceano","Bosco","Prugna"}) {
            preferences.preset=preset;
            QCOMPARE(NetLinkAppearance::makePalette(preferences,false).color(QPalette::Highlight),NetLinkAppearance::presetColor(preset));
        }
    }
    void arbitraryAccentContrast() {
        for(int red=0;red<256;red+=17) for(int green=0;green<256;green+=17) for(int blue=0;blue<256;blue+=17) {
            const QColor background(red,green,blue);const auto foreground=NetLinkAppearance::contrastingText(background);
            const auto a=luminance(background),b=luminance(foreground);
            QVERIFY((qMax(a,b)+.05)/(qMin(a,b)+.05)>=4.5);
        }
    }
    void cancelPreviewDoesNotSave() {
        QTemporaryDir directory;QVERIFY(directory.isValid());QSettings settings(directory.filePath("cancel.ini"),QSettings::IniFormat);
        NetLinkAppearance::Preferences preferences;preferences.mode="Light";QVERIFY(NetLinkAppearance::save(settings,preferences));
        auto manager=NetLinkAppearance::install(&settings);const auto initial=qApp->palette();
        QTimer::singleShot(0,this,[] {
            auto dialog=qApp->findChild<QDialog*>("netLinkAppearanceDialog");
            // The dialog is a top-level object rather than an application child.
            if(!dialog) for(auto widget:QApplication::topLevelWidgets()) if(widget->objectName()=="netLinkAppearanceDialog") dialog=qobject_cast<QDialog*>(widget);
            QVERIFY(dialog);dialog->findChild<QComboBox*>("appearanceMode")->setCurrentIndex(2);
            dialog->reject();
        });
        manager->showSettings(nullptr);
        QCOMPARE(qApp->palette(),initial);QCOMPARE(NetLinkAppearance::load(settings).mode,QString("Light"));delete manager;
    }
    void savePreviewAndReopen() {
        QTemporaryDir directory;QVERIFY(directory.isValid());const auto path=directory.filePath("save.ini");QSettings settings(path,QSettings::IniFormat);
        auto manager=NetLinkAppearance::install(&settings);
        QTimer::singleShot(0,this,[] {
            QDialog* dialog=nullptr;for(auto widget:QApplication::topLevelWidgets()) if(widget->objectName()=="netLinkAppearanceDialog") dialog=qobject_cast<QDialog*>(widget);
            QVERIFY(dialog);dialog->findChild<QComboBox*>("appearanceMode")->setCurrentIndex(2);
            dialog->findChild<QComboBox*>("appearancePreset")->setCurrentText("Bosco");
            const auto output=qEnvironmentVariable("NETLINK_APPEARANCE_SCREENSHOTS");
            if(!output.isEmpty()) {QDir().mkpath(output);QVERIFY(dialog->grab().save(QDir(output).filePath("netlink-appearance-dark.png")));}
            dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();
        });
        manager->showSettings(nullptr);delete manager;
        QSettings reopened(path,QSettings::IniFormat);const auto loaded=NetLinkAppearance::load(reopened);
        QCOMPARE(loaded.mode,QString("Dark"));QCOMPARE(loaded.preset,QString("Bosco"));QCOMPARE(loaded.accent,QColor("#0f766e"));
    }
};
QTEST_MAIN(NetLinkAppearanceTests)
#include "NetLinkAppearanceTests.moc"
