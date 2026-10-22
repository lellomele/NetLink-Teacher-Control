// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025-26 prof. Ing. Raffaele Mele
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include <QtTest>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <cmath>
#include <memory>
#include "NetLinkAppearance.h"
#include "NetLinkUi.h"

class NetLinkAppearanceTests : public QObject {
    Q_OBJECT
    static double luminance(const QColor& color) {
        const auto linear=[](double value){return value<=.04045?value/12.92:std::pow((value+.055)/1.055,2.4);};
        return .2126*linear(color.redF())+.7152*linear(color.greenF())+.0722*linear(color.blueF());
    }
private Q_SLOTS:
    void initTestCase() { QApplication::setFont(QFont("Segoe UI",10)); }
    void preferenceRoundTrip() {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        const auto path=directory.filePath("appearance.ini");
        QSettings settings(path,QSettings::IniFormat);
        NetLinkAppearance::Preferences preferences;preferences.mode="Dark";preferences.preset="Personalizzato";preferences.accent=QColor("#d3a011");
        preferences.iconSet="Outline";preferences.toolbarIconSize=44;
        settings.setValue("unrelated","preserved");
        QVERIFY(NetLinkAppearance::save(settings,preferences));
        QSettings reopened(path,QSettings::IniFormat);const auto loaded=NetLinkAppearance::load(reopened);
        QCOMPARE(loaded.mode,preferences.mode);QCOMPARE(loaded.preset,preferences.preset);QCOMPARE(loaded.accent,preferences.accent);
        QCOMPARE(loaded.iconSet,preferences.iconSet);QCOMPARE(loaded.toolbarIconSize,preferences.toolbarIconSize);
        QCOMPARE(reopened.value("unrelated").toString(),QString("preserved"));
    }
    void invalidPreferencesFallBack() {
        NetLinkAppearance::Preferences preferences;preferences.mode="invalid";preferences.preset="invalid";preferences.accent=QColor();
        preferences.iconSet="../../custom.dll";preferences.toolbarIconSize=4096;
        preferences=NetLinkAppearance::normalized(preferences);
        QCOMPARE(preferences.mode,QString("System"));QCOMPARE(preferences.preset,QString("Oceano"));QCOMPARE(preferences.accent,QColor("#2563eb"));
        QCOMPARE(preferences.iconSet,QString("Organic"));QCOMPARE(preferences.toolbarIconSize,36);
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
    void buttonPressFeedback_data() {
        QTest::addColumn<QString>("mode");QTest::addColumn<QString>("kind");QTest::addColumn<bool>("checked");
        for(const auto& mode:{"Light","Dark"}) for(const auto& kind:{"panel","default","utility","feature","compact"}) for(bool checked:{false,true})
            QTest::newRow(qPrintable(QString("%1-%2-checked-%3").arg(mode,kind).arg(checked)))<<QString(mode)<<QString(kind)<<checked;
    }
    void buttonPressFeedback() {
        QFETCH(QString,mode);QFETCH(QString,kind);QFETCH(bool,checked);
        QTemporaryDir directory;QVERIFY(directory.isValid());QSettings settings(directory.filePath("press.ini"),QSettings::IniFormat);
        NetLinkAppearance::Preferences preferences;preferences.mode=mode;QVERIFY(NetLinkAppearance::save(settings,preferences));
        std::unique_ptr<NetLinkAppearance> appearance(NetLinkAppearance::install(&settings));
        QWidget panel;auto layout=new QVBoxLayout(&panel);QAbstractButton* button=nullptr;
        if(kind=="panel" || kind=="default") {
            auto push=new QPushButton("Save JSON",&panel);push->setDefault(kind=="default");button=push;
        } else {
            auto tool=new QToolButton(&panel);tool->setText("Control");
            if(kind=="feature") tool->setProperty("netlinkFeatureButton",true);
            if(kind=="compact") tool->setProperty("netlinkCompactButton",true);
            button=tool;
        }
        button->setCheckable(checked);button->setChecked(checked);button->setMinimumSize(160,52);
        layout->addWidget(button);panel.resize(240,110);panel.show();button->setFocus();QTest::qWait(20);
        const auto geometry=button->geometry();const auto hint=button->sizeHint();
        QSignalSpy clicked(button,&QAbstractButton::clicked);
        const auto normal=button->grab().toImage();
        QTest::mousePress(button,Qt::LeftButton);QVERIFY(button->isDown());QCOMPARE(clicked.count(),0);
        const auto pressed=button->grab().toImage();QVERIFY(normal!=pressed);
        QCOMPARE(button->geometry(),geometry);QCOMPARE(button->sizeHint(),hint);
        if(!qApp->property("netlinkHighContrast").toBool()) {
            const QColor fill(qApp->palette().color(QPalette::Highlight).darker(125).name());int pixels=0;
            for(int y=0;y<pressed.height();++y) for(int x=0;x<pressed.width();++x) if(pressed.pixelColor(x,y)==fill) ++pixels;
            // Unstyled utility buttons retain Fusion's native sunken gradient;
            // flat themed buttons must use the exact stylesheet fill.
            if(kind!="utility") QVERIFY2(pixels>30,qPrintable(QString("%1 expected %2, observed %3").arg(kind,fill.name(),pressed.pixelColor(10,10).name())));
            const auto fg=NetLinkAppearance::contrastingText(fill);const auto a=luminance(fill),b=luminance(fg);
            QVERIFY((qMax(a,b)+.05)/(qMin(a,b)+.05)>=4.5);
        }
        const auto output=qEnvironmentVariable("NETLINK_PRESS_SCREENSHOTS");
        if(!output.isEmpty()) {QVERIFY(QDir().mkpath(output));QVERIFY(button->grab().save(QDir(output).filePath(QString("%1-%2-pressed-scale-%3.png").arg(mode,kind,qEnvironmentVariable("QT_SCALE_FACTOR","1")))));}
        QTest::mouseRelease(button,Qt::LeftButton);QVERIFY(!button->isDown());QCOMPARE(clicked.count(),1);
        if(checked) QVERIFY(!button->isChecked());
        QTest::keyPress(button,Qt::Key_Space);QVERIFY(button->isDown());QCOMPARE(clicked.count(),1);
        QTest::keyRelease(button,Qt::Key_Space);QVERIFY(!button->isDown());QCOMPARE(clicked.count(),2);
        button->setEnabled(false);QTest::mouseClick(button,Qt::LeftButton);QCOMPARE(clicked.count(),2);QVERIFY(!button->isDown());
    }
    void cancelPreviewDoesNotSave() {
        QTemporaryDir directory;QVERIFY(directory.isValid());QSettings settings(directory.filePath("cancel.ini"),QSettings::IniFormat);
        NetLinkAppearance::Preferences preferences;preferences.mode="Light";QVERIFY(NetLinkAppearance::save(settings,preferences));
        auto manager=NetLinkAppearance::install(&settings);const auto initial=qApp->palette();
        QTimer::singleShot(0,this,[] {
            auto dialog=qApp->findChild<QDialog*>("netLinkAppearanceDialog");
            // The dialog is a top-level object rather than an application child.
            if(!dialog) for(auto widget:QApplication::topLevelWidgets()) if(widget->objectName()=="netLinkAppearanceDialog") dialog=qobject_cast<QDialog*>(widget);
            QVERIFY(dialog);QVERIFY(!dialog->windowTitle().contains(NetLinkUi::copyrightText()));
            for(auto label:dialog->findChildren<QLabel*>()) QVERIFY(label->text()!=NetLinkUi::copyrightText());
            dialog->findChild<QComboBox*>("appearanceMode")->setCurrentIndex(2);
            dialog->findChild<QComboBox*>("appearanceIconSet")->setCurrentIndex(1);
            dialog->findChild<QComboBox*>("appearanceIconSize")->setCurrentIndex(2);
            dialog->reject();
        });
        manager->showSettings(nullptr);
        QCOMPARE(qApp->palette(),initial);QCOMPARE(NetLinkAppearance::load(settings).mode,QString("Light"));delete manager;
        QCOMPARE(NetLinkAppearance::load(settings).iconSet,QString("Organic"));QCOMPARE(NetLinkAppearance::load(settings).toolbarIconSize,36);
    }
    void savePreviewAndReopen() {
        QTemporaryDir directory;QVERIFY(directory.isValid());const auto path=directory.filePath("save.ini");QSettings settings(path,QSettings::IniFormat);
        auto manager=NetLinkAppearance::install(&settings);
        QTimer::singleShot(0,this,[] {
            QDialog* dialog=nullptr;for(auto widget:QApplication::topLevelWidgets()) if(widget->objectName()=="netLinkAppearanceDialog") dialog=qobject_cast<QDialog*>(widget);
            QVERIFY(dialog);dialog->findChild<QComboBox*>("appearanceMode")->setCurrentIndex(2);
            auto preset=dialog->findChild<QComboBox*>("appearancePreset");preset->setCurrentIndex(preset->findData("Bosco"));
            dialog->findChild<QComboBox*>("appearanceIconSet")->setCurrentIndex(1);
            dialog->findChild<QComboBox*>("appearanceIconSize")->setCurrentIndex(2);
            const auto output=qEnvironmentVariable("NETLINK_APPEARANCE_SCREENSHOTS");
            if(!output.isEmpty()) {QDir().mkpath(output);QVERIFY(dialog->grab().save(QDir(output).filePath("netlink-appearance-dark.png")));}
            dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();
        });
        manager->showSettings(nullptr);delete manager;
        QSettings reopened(path,QSettings::IniFormat);const auto loaded=NetLinkAppearance::load(reopened);
        QCOMPARE(loaded.mode,QString("Dark"));QCOMPARE(loaded.preset,QString("Bosco"));QCOMPARE(loaded.accent,QColor("#0f766e"));
        QCOMPARE(loaded.iconSet,QString("Outline"));QCOMPARE(loaded.toolbarIconSize,44);
    }
};
QTEST_MAIN(NetLinkAppearanceTests)
#include "NetLinkAppearanceTests.moc"
