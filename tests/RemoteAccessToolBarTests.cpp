// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include <QtTest>
#include <QDir>
#include <QComboBox>
#include <QDialog>
#include <QMenu>
#include <QPointer>
#include <QSettings>
#include <QTemporaryDir>
#include <QToolButton>
#include "RemoteAccessToolBar.h"
#include "NetLinkAppearance.h"
#include "NetLinkUi.h"

class RemoteAccessToolBarTests : public QObject
{
    Q_OBJECT
    static QToolButton* button(QWidget& bar, const char* name) { return bar.findChild<QToolButton*>(name); }
    static void leave(RemoteAccessToolBar& bar) {
        bar.parentWidget()->setFocusPolicy(Qt::StrongFocus);
        bar.parentWidget()->setFocus();QApplication::processEvents();
        if(auto focus=QApplication::focusWidget()) focus->clearFocus();
        QEvent event(QEvent::Leave); QApplication::sendEvent(&bar,&event); bar.disappear();
    }
    static void verifyButtons(RemoteAccessToolBar& bar) {
        const auto panel=bar.findChild<QWidget*>("remoteToolbarPanel");
        QVERIFY(panel);
        for(auto control:panel->findChildren<QToolButton*>()) if(!control->isHidden()) {
            QVERIFY2(!control->icon().isNull(),qPrintable(control->objectName()));
            QCOMPARE(control->size(),QSize(44,44));
            QCOMPARE(control->toolButtonStyle(),Qt::ToolButtonIconOnly);
            QCOMPARE(control->property("netlinkCompactButton").toBool(),true);
            QVERIFY(!control->accessibleName().isEmpty()); QVERIFY(!control->toolTip().isEmpty());
            QVERIFY2(bar.rect().contains(QRect(control->mapTo(&bar,QPoint()),control->size())),qPrintable(control->objectName()));
        }
    }
private Q_SLOTS:
    void initTestCase() { QApplication::setFont(QFont("Segoe UI",10)); QLocale::setDefault(QLocale("it_IT")); }
    void compactAndDesktopNotDimmed() {
        QWidget desktop; desktop.resize(1100,650); desktop.setAutoFillBackground(true);
        auto palette=desktop.palette(); palette.setColor(QPalette::Window,QColor("#83b6ce"));desktop.setPalette(palette);
        const auto original=desktop.grab().toImage();
        RemoteAccessToolBar bar(&desktop,false,true);desktop.show();bar.appear();QApplication::processEvents();
        QCOMPARE(bar.height(),56); QVERIFY(bar.width()<450); QCOMPARE(bar.y(),8);
        QCOMPARE(bar.x(),(desktop.width()-bar.width())/2);
        verifyButtons(bar);
        QVERIFY(!bar.isWindow()); QVERIFY(!bar.isModal());
        const auto image=desktop.grab().toImage();
        const auto dpr=image.devicePixelRatio();
        for(const auto p:{QPoint(20,20),QPoint(600,400),QPoint(1070,620)}) {
            QCOMPARE(image.pixelColor(p*dpr),original.pixelColor(p*dpr));
            QVERIFY(desktop.childAt(p)==nullptr);
        }
    }
    void pinUnpinAndCancellableHide() {
        QWidget desktop;desktop.resize(900,600);RemoteAccessToolBar bar(&desktop,false,true);desktop.show();
        leave(bar);QTRY_VERIFY_WITH_TIMEOUT(!bar.isExpanded(),1400);
        bar.appear();leave(bar);QTest::qWait(200);
        bar.appear();bar.setPinned(true);leave(bar);QTest::qWait(800);
        QVERIFY(bar.isExpanded());QVERIFY(bar.isPinned());
        QCOMPARE(button(bar,"remotePin")->isChecked(),true);
        bar.setPinned(false);leave(bar);QTRY_VERIFY_WITH_TIMEOUT(!bar.isExpanded(),1400);
        QCOMPARE(bar.size(),QSize(64,16));
        button(bar,"remoteToolbarHandle")->click();QVERIFY(bar.isExpanded());
    }
    void edgeHoverAndCancellation() {
        QWidget desktop;desktop.resize(900,600);RemoteAccessToolBar bar(&desktop,false,true);desktop.show();
        leave(bar);QTRY_VERIFY_WITH_TIMEOUT(!bar.isExpanded(),1400);
        bar.requestRevealAt({0,200});bar.requestRevealAt({200,200});QTest::qWait(450);QVERIFY(!bar.isExpanded());
        const QList<QPair<QPoint,Qt::Edge>> edges{{{400,0},Qt::TopEdge},{{400,599},Qt::BottomEdge},{{0,250},Qt::LeftEdge},{{899,250},Qt::RightEdge}};
        for(const auto& entry:edges) {
            bar.requestRevealAt(entry.first);QTest::qWait(100);QVERIFY(!bar.isExpanded());
            QTRY_VERIFY_WITH_TIMEOUT(bar.isExpanded(),800);QCOMPARE(bar.dockEdge(),entry.second);
            verifyButtons(bar);QVERIFY(desktop.rect().contains(bar.geometry()));
            if(entry.second==Qt::LeftEdge || entry.second==Qt::RightEdge) QCOMPARE(bar.width(),56);
            else QCOMPARE(bar.height(),56);
            leave(bar);QTRY_VERIFY_WITH_TIMEOUT(!bar.isExpanded(),1400);
        }
    }
    void popupAndKeyboardFocusPreventHiding() {
        QWidget desktop;desktop.resize(900,600);RemoteAccessToolBar bar(&desktop,false,true);desktop.show();bar.appear();
        auto screen=button(bar,"remoteScreens");auto menu=screen->menu();menu->addAction("Monitor 1");
        leave(bar);menu->popup(desktop.mapToGlobal({100,100}));QTest::qWait(800);QVERIFY(bar.isExpanded());
        menu->hide();leave(bar);QTRY_VERIFY_WITH_TIMEOUT(!bar.isExpanded(),1400);
        bar.appear();button(bar,"remotePin")->setFocus();bar.disappear();QTest::qWait(800);QVERIFY(bar.isExpanded());
        leave(bar);QTRY_VERIFY_WITH_TIMEOUT(!bar.isExpanded(),1400);
    }
    void actionsAndViewOnly() {
        QWidget desktop;desktop.resize(900,600);RemoteAccessToolBar bar(&desktop,false,true);desktop.show();bar.appear();
        QSignalSpy shots(&bar,&RemoteAccessToolBar::screenshotRequested),close(&bar,&RemoteAccessToolBar::disconnectRequested),
            full(&bar,&RemoteAccessToolBar::fullScreenChanged),mode(&bar,&RemoteAccessToolBar::viewOnlyChanged);
        button(bar,"remoteScreenshot")->click();QCOMPARE(shots.count(),1);
        button(bar,"remoteFullscreen")->click();QCOMPARE(full.count(),1);QCOMPARE(full.first().first().toBool(),true);
        button(bar,"remoteViewOnly")->click();QCOMPARE(mode.count(),1);QVERIFY(button(bar,"remoteShortcuts")->isHidden());
        bar.updateControls(false);QVERIFY(!button(bar,"remoteShortcuts")->isHidden());QCOMPARE(mode.count(),1);
        button(bar,"remotePin")->click();QVERIFY(bar.isPinned());
        button(bar,"remoteDisconnect")->click();QCOMPARE(close.count(),1);
        RemoteAccessToolBar viewOnly(&desktop,true,false);QVERIFY(!button(viewOnly,"remoteViewOnly"));
        QVERIFY(button(viewOnly,"remoteShortcuts")->isHidden());
    }
    void resizeThemeAndPreview() {
        QTemporaryDir dir;QSettings settings(dir.filePath("appearance.ini"),QSettings::IniFormat);
        QWidget desktop;desktop.resize(1000,600);desktop.show();
        RemoteAccessToolBar bar(&desktop,false,true);bar.setPinned(true);
        for(const auto size:{QSize(1000,600),QSize(360,230),QSize(240,180),QSize(600,1000)}) {
            desktop.resize(size);QApplication::processEvents();
            for(const auto edge:{Qt::TopEdge,Qt::BottomEdge,Qt::LeftEdge,Qt::RightEdge}) {
                bar.setDockEdge(edge);QApplication::processEvents();
                QVERIFY(desktop.rect().contains(bar.geometry()));verifyButtons(bar);QVERIFY(bar.isPinned());
            }
        }
        desktop.hide();
        for(const auto mode:{"Light","Dark"}) for(const auto icons:{"Organic","Outline","Spectrum","Classic"}) {
            NetLinkAppearance::Preferences p;p.mode=mode;p.iconSet=icons;
            QVERIFY(NetLinkAppearance::save(settings,p));auto appearance=NetLinkAppearance::install(&settings);
            QWidget preview;preview.resize(1000,600);RemoteAccessToolBar previewBar(&preview,false,true);previewBar.setPinned(true);
            preview.show();QApplication::processEvents();verifyButtons(previewBar);
            const auto actual=button(previewBar,"remoteScreenshot")->icon().pixmap(28,28).toImage();
            const auto expected=NetLinkUi::icon("Screenshot",icons,p.accent,QIcon(":/remoteaccess/camera-photo.png")).pixmap(28,28).toImage();
            QCOMPARE(actual,expected);
            const auto output=qEnvironmentVariable("NETLINK_REMOTE_SCREENSHOTS");
            if(!output.isEmpty()) {
                QVERIFY(QDir().mkpath(output));
                const auto path=QDir(output).filePath(QString("remote-%1-%2-%3.png").arg(mode,icons).arg(qRound(preview.devicePixelRatioF()*100)));
                QVERIFY(preview.grab().save(path));
            }
            delete appearance;
        }
    }
    void liveAppearanceChangesAndCancelRestoresIcons() {
        QTemporaryDir dir;QSettings settings(dir.filePath("live.ini"),QSettings::IniFormat);
        NetLinkAppearance::Preferences initial;initial.iconSet="Organic";initial.mode="Light";
        QVERIFY(NetLinkAppearance::save(settings,initial));auto appearance=NetLinkAppearance::install(&settings);
        QWidget desktop;desktop.resize(1000,600);RemoteAccessToolBar bar(&desktop,false,true);bar.setPinned(true);desktop.show();
        const auto original=button(bar,"remoteScreenshot")->icon().pixmap(28,28).toImage();
        QTimer::singleShot(0,this,[&] {
            QDialog* dialog=nullptr;for(auto widget:QApplication::topLevelWidgets()) if(widget->objectName()=="netLinkAppearanceDialog") dialog=qobject_cast<QDialog*>(widget);
            QVERIFY(dialog);auto sets=dialog->findChild<QComboBox*>("appearanceIconSet");
            auto modes=dialog->findChild<QComboBox*>("appearanceMode");
            for(const auto mode:{"Dark","Light"}) for(const auto set:{"Spectrum","Outline","Classic","Organic"}) {
                modes->setCurrentIndex(modes->findData(mode));sets->setCurrentIndex(sets->findData(set));
                QApplication::processEvents();verifyButtons(bar);
                const auto prefs=appearance->preferences();
                QCOMPARE(button(bar,"remoteScreenshot")->icon().pixmap(28,28).toImage(),
                    NetLinkUi::icon("Screenshot",set,prefs.accent,QIcon(":/remoteaccess/camera-photo.png")).pixmap(28,28).toImage());
                QVERIFY(bar.isPinned());QVERIFY(bar.isExpanded());
            }
            sets->setCurrentIndex(sets->findData("Spectrum"));dialog->reject();
        });
        appearance->showSettings(&desktop);
        QCOMPARE(button(bar,"remoteScreenshot")->icon().pixmap(28,28).toImage(),original);
        delete appearance;
    }
    void pendingTimersAreOwnedAndDoNotOutliveWindow() {
        QWidget desktop;desktop.resize(900,600);auto bar=new RemoteAccessToolBar(&desktop,false,true);desktop.show();
        leave(*bar);QTRY_VERIFY_WITH_TIMEOUT(!bar->isExpanded(),1400);bar->requestRevealAt({0,200});
        QPointer<RemoteAccessToolBar> pointer(bar);delete bar;QVERIFY(pointer.isNull());QTest::qWait(750);
    }
    void languagesUseSharedCatalog() {
        for(const auto locale:{"en_US","it_IT"}) {
            QLocale::setDefault(QLocale(locale));QWidget desktop;RemoteAccessToolBar bar(&desktop,false,true);
            QCOMPARE(button(bar,"remotePin")->text(),QString(locale==QString("it_IT")?"Fissa barra":"Pin toolbar"));
            QCOMPARE(button(bar,"remoteDisconnect")->text(),QString(locale==QString("it_IT")?"Disconnetti":"Disconnect"));
        }
    }
};
int main(int argc,char** argv) {
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    QApplication app(argc,argv);RemoteAccessToolBarTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "RemoteAccessToolBarTests.moc"
