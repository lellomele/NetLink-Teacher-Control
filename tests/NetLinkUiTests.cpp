// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include <QtTest>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QGridLayout>
#include <QGroupBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <memory>
#include "NetLinkUi.h"
#include "NetLinkAppearance.h"
#include "TranslationLoader.h"
#include "LockWidget.h"
#include "Feature.h"
#include "ToolButton.h"
#include "SchoolConfigurationPage.h"
#include "VeyonConfiguration.h"
#include "Configuration/JsonStore.h"
#include "BuiltinDirectoryConfiguration.h"
#include "BuiltinDirectoryConfigurationPage.h"
#include "NetworkObject.h"

// Actual directory page with an isolated model: no VeyonCore initialization,
// key generation, service launch, WFP or network connections in this suite.
IMPLEMENT_CONFIG_PROXY(BuiltinDirectoryConfiguration)

class NetLinkUiTests : public QObject {
    Q_OBJECT
    QTemporaryDir m_directory;
    std::unique_ptr<QSettings> m_settings;
    NetLinkAppearance* m_appearance=nullptr;
    QLocale m_initialLocale;
    QJsonArray objects() {
        const auto group=QUuid("40c9cbb7-aee2-44c8-9d62-895e053c033a");
        return {NetworkObject(NetworkObject::Type::Location,"Aula Wi-Fi",{},{},{},group).toJson(),
            NetworkObject(NetworkObject::Type::Host,"PC-01","192.0.2.11",{},{},QUuid::createUuid(),group).toJson(),
            NetworkObject(NetworkObject::Type::Host,"PC-02","192.0.2.12",{},{},QUuid::createUuid(),group).toJson()};
    }
    void snapshot(QWidget& widget,const QString& name) {
        const auto output=qEnvironmentVariable("NETLINK_UI_SCREENSHOTS");if(output.isEmpty()) return;
        QVERIFY(QDir().mkpath(output));
        const auto path=QDir(output).filePath(name+QString("-%1.png").arg(qRound(widget.devicePixelRatioF()*100)));
        QVERIFY(widget.grab().save(path));
    }
private Q_SLOTS:
    void initTestCase() {
        QVERIFY(m_directory.isValid());
        QLocale::setDefault(QLocale("it_IT"));
        QApplication::setFont(QFont("Segoe UI",10));
        m_settings.reset(new QSettings(m_directory.filePath("appearance.ini"),QSettings::IniFormat));
        NetLinkAppearance::Preferences p;p.mode="Light";p.preset="Bosco";
        QVERIFY(NetLinkAppearance::save(*m_settings,p));m_appearance=NetLinkAppearance::install(m_settings.get());
    }
    void cleanupTestCase() { delete m_appearance;m_settings.reset();QLocale::setDefault(m_initialLocale); }
    void legacyBrandingDoesNotOverrideNetLink() {
        QCOMPARE(NetLinkUi::displayVersion("4.7.5-netlink.1.0.0-alpha.18"),QString("1.0.0-alpha.18"));
        QCOMPARE(NetLinkUi::displayVersion("4.7.5-netlink.0.1.0-alpha.16"),QString("0.1.0-alpha.16"));
        QCOMPARE(NetLinkUi::displayVersion("4.7.5"),QString("4.7.5"));
        for(const auto& name:{"","Veyon","Veyon School"," vEyOn "}) QCOMPARE(NetLinkUi::applicationName(name),QString("NetLink"));
        QCOMPARE(NetLinkUi::applicationName("My school"),QString("My school"));
    }
    void selectedLanguageWinsOverWindowsLocaleAndCatalogIsComplete() {
        struct Restore {QLocale value;~Restore(){QLocale::setDefault(value);}} restore{QLocale()};
        QCOMPARE(TranslationLoader::localeForConfiguration("English - American English (en_US)",QLocale("it_IT")).name(),QString("en_US"));
        QCOMPARE(TranslationLoader::localeForConfiguration("Italian - italiano (it_IT)",QLocale("en_US")).language(),QLocale::Italian);
        QCOMPARE(TranslationLoader::localeForConfiguration("",QLocale("it_IT")).language(),QLocale::Italian);
        QFile file(":/core/netlink-ui.json");QVERIFY(file.open(QIODevice::ReadOnly));
        const auto entries=QJsonDocument::fromJson(file.readAll()).array();QVERIFY(entries.size()>=166);
        for(const auto& entry:entries) {
            const auto object=entry.toObject();const auto source=object["source"].toString(),italian=object["it"].toString();
            QVERIFY(!source.isEmpty());QVERIFY(!italian.isEmpty());
            QLocale::setDefault(QLocale("en_US"));QCOMPARE(NetLinkUi::text(source),source);
            QLocale::setDefault(QLocale("it_IT"));QCOMPARE(NetLinkUi::text(source),italian);
        }
        QLocale::setDefault(QLocale("en_US"));
        NetLinkUi::installStandardTranslations();
        QCOMPARE(QCoreApplication::translate("QPlatformTheme","&Yes"),QString("&Yes"));
        QMainWindow window;NetLinkUi::addInformationMenu(&window);
        QCOMPARE(window.findChild<QAction*>("netlinkAppearanceAction")->text(),QString::fromUtf8("Appearance, colors and icons…"));
        Feature attendance("Appello",Feature::Flag::Action,QUuid::createUuid(),{},"Attendance",{},{});
        QVERIFY(NetLinkUi::featureDescription(attendance).startsWith("Asks students"));
        QLocale::setDefault(QLocale("it_IT"));
        QCOMPARE(QCoreApplication::translate("QPlatformTheme","&Yes"),QString::fromUtf8("&Sì"));
    }
    void softwareLockIgnoresWindowCloseAndRestoresWithoutSystemChanges() {
        LockWidget cover(LockWidget::BackgroundPixmap,{},nullptr,false);
        QVERIFY(!cover.testAttribute(Qt::WA_QuitOnClose));QVERIFY(cover.isVisible());
        QVERIFY(!cover.close());QVERIFY(cover.isVisible());
        cover.hide();QVERIFY(!cover.isVisible());cover.restoreWindow();QVERIFY(cover.isVisible());
    }
    void copyrightUsesUnicodeAndOnlyOneNativeTitleSuffix() {
        QCOMPARE(NetLinkUi::copyrightText(),QString::fromUtf8("© 2026 prof. Ing. Raffaele Mele"));
        const auto title=NetLinkUi::windowTitle("NetLink — Console docente");
        QVERIFY(title.startsWith("NetLink — Console docente"));
        QVERIFY(title.endsWith(NetLinkUi::copyrightText()));
        QCOMPARE(NetLinkUi::windowTitle(title),title);
        QVERIFY(!title.contains("(C)"));
    }
    void vectorSetsRenderAtEverySize() {
        for(const auto& set:{"Organic","Outline","Spectrum"}) for(const auto& name:{"MonitoringMode","InternetGuard","Appello","Discovery","RemoteControl","RemoteView","Demo","ScreenLock","TextMessage","Screenshot","PowerOn","PowerDown","Reboot","StartApp","OpenWebsite","ClipboardExchange","FileTransfer","UserLogon","UserLogoff","add","remove","settings","info","key","group"}) {
            const auto icon=NetLinkUi::icon(name,set,QColor("#0f766e"));QVERIFY(!icon.isNull());
            for(int size:{16,32,64,128}) for(auto mode:{QIcon::Normal,QIcon::Disabled,QIcon::Selected}) {
                const auto image=icon.pixmap(size,size,mode).toImage();QVERIFY(!image.isNull());
                bool drawn=false;for(int y=0;y<image.height() && !drawn;++y) for(int x=0;x<image.width();++x) if(qAlpha(image.pixel(x,y))){drawn=true;break;}
                QVERIFY(drawn);
            }
        }
        QPixmap legacy(24,24);legacy.fill(Qt::red);const QIcon old(legacy);
        QCOMPARE(NetLinkUi::icon("MonitoringMode","Classic",Qt::blue,old).cacheKey(),old.cacheKey());
        QCOMPARE(NetLinkUi::icon("UnknownPlugin","Organic",Qt::blue,old).cacheKey(),old.cacheKey());
    }
    void fullToolbarUsesOverflowWithoutElidingLabels() {
        struct Restore {QLocale value;~Restore(){QLocale::setDefault(value);}} restore{QLocale()};QLocale::setDefault(QLocale("en_US"));
        QMainWindow window;window.resize(1120,360);auto bar=window.addToolBar("Complete toolbar");
        bar->setIconSize(QSize(36,36));
        const QStringList roles{"MonitoringMode","Demo","ScreenLock","RemoteView","RemoteControl","PowerOn","Reboot","PowerDown","UserLogin","UserLogoff","TextMessage","StartApp","OpenWebsite","FileTransfer","Screenshot","InternetGuard","Appello","Discovery"};int index=0;
        for(const auto& label:{"Monitoring","Demonstration","Lock","Remote view","Remote control","Power on","Restart","Power down","Log on","Log off","Text message","Start application","Open website","File transfer","Screenshot","InternetGuard","Attendance","Find clients"}) {
            const auto role=roles[index++];auto button=new ToolButton(NetLinkUi::icon(role,"Spectrum",Qt::blue),label);button->setProperty("testRole",role);
            button->addTo(bar);
        }
        window.show();QApplication::processEvents();
        int visible=0;for(auto button:bar->findChildren<ToolButton*>()) if(button->isVisible()) {
            ++visible;QVERIFY2(button->width()>=button->minimumSizeHint().width(),qPrintable(button->text()));
        }
        QVERIFY(visible>0);QVERIFY(visible<18);QCOMPARE(bar->actions().size(),18);
        auto extension=bar->findChild<QToolButton*>("qt_toolbar_ext_button");QVERIFY(extension);QVERIFY(extension->isVisible());
        snapshot(window,"netlink-full-toolbar");
        bool overflowAvailable=false;
        QTimer::singleShot(0,&window,[&] {if(auto menu=qobject_cast<QMenu*>(QApplication::activePopupWidget())){overflowAvailable=!menu->actions().isEmpty();menu->close();}});
        QTimer::singleShot(100,&window,[]{if(auto menu=qobject_cast<QMenu*>(QApplication::activePopupWidget())) menu->close();});
        // Qt expands a docked toolbar into rows when there is enough room;
        // only floating/short toolbars use a popup menu. Verify both paths.
        const bool usesMenu=extension->menu()!=nullptr;
        QTest::mouseClick(extension,Qt::LeftButton);QApplication::processEvents();
        if(usesMenu) QVERIFY(overflowAvailable);
        else {
            QTRY_VERIFY(bar->findChildren<ToolButton*>().last()->isVisible());
            for(auto button:bar->findChildren<ToolButton*>()) QVERIFY(button->width()>=button->minimumSizeHint().width());
            QTest::mouseClick(extension,Qt::LeftButton);
        }
        QTimer::singleShot(0,&window,[&] {
            QDialog* dialog=nullptr;for(auto widget:QApplication::topLevelWidgets()) if(widget->objectName()=="netLinkAppearanceDialog") dialog=qobject_cast<QDialog*>(widget);
            QVERIFY(dialog);auto mode=dialog->findChild<QComboBox*>("appearanceMode");mode->setCurrentIndex(mode->findData("Dark"));
            for(auto button:bar->findChildren<ToolButton*>()) button->setIcon(NetLinkUi::icon(button->property("testRole").toString(),"Spectrum",Qt::blue));
            window.resize(1680,360);QApplication::processEvents();QTest::qWait(250);snapshot(window,"netlink-spectrum-dark");dialog->reject();
        });
        m_appearance->showSettings(&window);
    }
    void spectrumHasDistinctColorsAndRespectsHighContrast() {
        const auto primary=[](const QImage& image) {
            QHash<QRgb,int> counts;for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x) if(qAlpha(image.pixel(x,y))==255) ++counts[image.pixel(x,y)];
            QRgb color=0;int maximum=0;for(auto i=counts.cbegin();i!=counts.cend();++i)if(i.value()>maximum){color=i.key();maximum=i.value();}return color;
        };
        QSet<QRgb> colors;
        for(const auto& role:{"MonitoringMode","Demo","ScreenLock","PowerOn","Reboot","Appello","Discovery","Screenshot"}) {
            const auto color=primary(NetLinkUi::icon(role,"Spectrum",Qt::blue).pixmap(64,64).toImage());
            QVERIFY(QColor::fromRgba(color).saturation()>50);colors.insert(color);
        }
        QVERIFY(colors.size()>=6);
        const auto previous=qApp->property("netlinkHighContrast");qApp->setProperty("netlinkHighContrast",true);
        const auto image=NetLinkUi::icon("Appello","Spectrum",Qt::red).pixmap(64,64).toImage();
        qApp->setProperty("netlinkHighContrast",previous);
        QCOMPARE(primary(image),qApp->palette().color(QPalette::ButtonText).rgba());
    }
    void shortToolbarPopupKeepsCommandsOperable() {
        QMainWindow window;window.resize(450,95);auto bar=window.addToolBar("Short toolbar");
        for(int i=0;i<18;++i) (new ToolButton(NetLinkUi::icon("ScreenLock","Spectrum",Qt::blue),QString("Command %1").arg(i)))->addTo(bar);
        window.show();QApplication::processEvents();
        auto extension=bar->findChild<QToolButton*>("qt_toolbar_ext_button");QVERIFY(extension);QVERIFY(extension->isVisible());
        QVERIFY(extension->menu());QVERIFY(extension->isEnabled());
        auto last=bar->findChildren<ToolButton*>().last();QVERIFY(!last->isVisible());QSignalSpy clicked(last,&QToolButton::clicked);
        bool found=false;QTimer::singleShot(0,&window,[&] {
            auto menu=qobject_cast<QMenu*>(QApplication::activePopupWidget());if(!menu) return;
            for(auto action:menu->actions()) if(action->text()==last->text()){found=true;action->trigger();break;}
            menu->close();
        });
        QTimer::singleShot(100,&window,[]{if(auto menu=qobject_cast<QMenu*>(QApplication::activePopupWidget())) menu->close();});
        QTest::mouseClick(extension,Qt::LeftButton);QVERIFY(found);QCOMPARE(clicked.size(),1);
        last->setEnabled(false);QVERIFY(!bar->actions().last()->isEnabled());
        last->setEnabled(true);last->setCheckable(true);last->setChecked(true);last->updateMinimumSize();
        QVERIFY(bar->actions().last()->isChecked());
        bar->setEnabled(false);bar->setEnabled(true);QVERIFY(last->isEnabled());QVERIFY(bar->actions().last()->isEnabled());
    }
    void buttonsAreAccessibleAndDoNotCreateHoverWindows() {
        QWidget parent;auto layout=new QVBoxLayout(&parent);
        auto button=new ToolButton(NetLinkUi::icon("Appello","Organic",Qt::blue),"Appello","Termina", "<script>nome</script> e identità",QKeySequence("Ctrl+E"));
        layout->addWidget(button);parent.show();QApplication::processEvents();
        QCOMPARE(button->iconSize(),QSize(36,36));QVERIFY(button->minimumWidth()>=44);
        const auto raster=button->icon().pixmap(parent.windowHandle(),QSize(36,36));
        QCOMPARE(raster.size(),QSize(36,36)*parent.devicePixelRatioF());
        QCOMPARE(raster.devicePixelRatioF(),parent.devicePixelRatioF());
        QCOMPARE(button->accessibleName(),QString("Appello"));QVERIFY(button->toolTip().contains("&lt;script&gt;"));
        QVERIFY(!button->toolTip().contains("<script>"));
        const auto topLevels=QApplication::topLevelWidgets().size();
        for(int i=0;i<100;++i){QEvent enter(QEvent::Enter);QApplication::sendEvent(button,&enter);QEvent leave(QEvent::Leave);QApplication::sendEvent(button,&leave);}
        QCOMPARE(QApplication::topLevelWidgets().size(),topLevels);
        button->setCheckable(true);button->setChecked(true);QCOMPARE(button->text(),QString("Termina"));
        ToolButton::setIconOnlyMode(&parent,true);QCOMPARE(button->toolButtonStyle(),Qt::ToolButtonIconOnly);
        ToolButton::setIconOnlyMode(&parent,false);QCOMPARE(button->toolButtonStyle(),Qt::ToolButtonTextUnderIcon);
    }
    void easyModeDoesNotMutateHiddenConfiguration() {
        Configuration::JsonStore store(Configuration::Store::System,m_directory.filePath("easy.json"));
        VeyonConfiguration config(&store);config.setSchoolResources({"192.0.2.20/32"});
        config.setSchoolDiscoveryMaster("192.0.2.1");SchoolConfigurationPage page(config);
        const auto before=config.data();QSignalSpy changed(&config,&VeyonConfiguration::configurationChanged);
        auto resources=page.findChild<QTextEdit*>("schoolResources");QVERIFY(resources);
        for(int i=0;i<100;++i){NetLinkUi::setEasyPage(&page,true);QVERIFY(resources->isHidden());NetLinkUi::setEasyPage(&page,false);QVERIFY(!resources->isHidden());}
        QCOMPARE(config.data(),before);QCOMPARE(changed.count(),0);
        for(const auto& name:{"GeneralConfigurationPage","ServiceConfigurationPage","AuthKeysConfigurationPage","BuiltinDirectoryConfigurationPage","SchoolConfigurationPage"}) QVERIFY(NetLinkUi::easyPage(name));
        for(const auto& name:{"AccessControlPage","MasterConfigurationPage","LdapConfigurationPage","ThirdPartyPlugin"}) QVERIFY(!NetLinkUi::easyPage(name));
    }
    void directoryControlsStayAboveTablesAndRowsDoNotReenter() {
        Configuration::JsonStore store(Configuration::Store::System,m_directory.filePath("directory.json"));
        VeyonConfiguration config(&store);BuiltinDirectoryConfiguration directory(&config);directory.setNetworkObjects(objects());
        QSignalSpy changed(&config,&VeyonConfiguration::configurationChanged);
        BuiltinDirectoryConfigurationPage page(directory);page.resize(780,400);page.show();QApplication::processEvents();
        auto groups=page.findChild<QTableWidget*>("locationTableWidget");auto computers=page.findChild<QTableWidget*>("computerTableWidget");
        QVERIFY(groups);QVERIFY(computers);QCOMPARE(groups->rowCount(),1);QCOMPARE(computers->rowCount(),2);QCOMPARE(changed.count(),0);
        for(const auto& name:{"addLocationButton","removeLocationButton","addComputerButton","removeComputerButton"}) {
            auto button=page.findChild<QPushButton*>(name);QVERIFY(button);QVERIFY(!button->text().isEmpty());
            QVERIFY(page.rect().contains(QRect(button->mapTo(&page,QPoint()),button->size())));
            QVERIFY(button->mapTo(&page,QPoint(0,button->height())).y()<=computers->mapTo(&page,QPoint()).y());
        }
        computers->setCurrentCell(0,1);changed.clear();computers->item(0,1)->setText("192.0.2.30");
        QCOMPARE(changed.count(),1);QCOMPARE(directory.networkObjects().size(),3);
        for(int i=0;i<25;++i) page.resetWidgets();
        QCOMPARE(changed.count(),1);QCOMPARE(groups->rowCount(),1);QCOMPARE(computers->rowCount(),2);
        snapshot(page,"netlink-gruppi");
        changed.clear();directory.setNetworkObjects({});page.resetWidgets();
        QCOMPARE(groups->rowCount(),0);QCOMPARE(computers->rowCount(),0);
        QVERIFY(!page.findChild<QPushButton*>("addComputerButton")->isEnabled());
        QVERIFY(!page.findChild<QPushButton*>("removeLocationButton")->isEnabled());
        QCOMPARE(changed.count(),1);
    }
    void removingAGroupRequiresExplicitConfirmation() {
        Configuration::JsonStore store(Configuration::Store::System,m_directory.filePath("remove.json"));
        VeyonConfiguration config(&store);BuiltinDirectoryConfiguration directory(&config);directory.setNetworkObjects(objects());
        BuiltinDirectoryConfigurationPage page(directory);const auto before=directory.networkObjects();
        for(bool accept:{false,true}) {
            QTimer::singleShot(0,&page,[accept]{
                auto box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());QVERIFY(box);
                QCOMPARE(box->standardButton(box->defaultButton()),QMessageBox::No);
                box->button(accept?QMessageBox::Yes:QMessageBox::No)->click();
            });
            QVERIFY(QMetaObject::invokeMethod(&page,"removeLocation",Qt::DirectConnection));
            if(!accept) QCOMPARE(directory.networkObjects(),before);else QVERIFY(directory.networkObjects().isEmpty());
        }
    }
    void traySummaryIsBoundedAndDoesNotClaimReachability() {
        for(bool listening:{false,true}) for(bool local:{false,true}) for(bool keys:{false,true}) for(bool discovery:{false,true}) {
            const auto text=NetLinkUi::traySummary(QString(400,'x')+"\n\u202e",65535,listening,local,keys,discovery,999999,999999);
            QVERIFY(text.size()<=127);QCOMPARE(text.count('\n'),3);QVERIFY(!text.contains(QChar(0x202e)));
            QVERIFY(!text.contains("raggiungibile"));QVERIFY(text.contains(keys?"Chiave":"Account"));
        }
        const auto text=NetLinkUi::traySummary("MATEBOOK",11100,true,false,true,true,1,0);
        QVERIFY(text.contains("in ascolto"));QVERIFY(text.contains("Accessi 1"));QVERIFY(text.contains("WSD config."));
    }
    void groupConfirmationKeepsTheOriginalTarget() {
        Configuration::JsonStore store(Configuration::Store::System,m_directory.filePath("selection.json"));
        VeyonConfiguration config(&store);BuiltinDirectoryConfiguration directory(&config);auto entries=objects();
        const auto other=QUuid::createUuid();entries.append(NetworkObject(NetworkObject::Type::Location,"Altro gruppo",{},{},{},other).toJson());directory.setNetworkObjects(entries);
        BuiltinDirectoryConfigurationPage page(directory);
        QTimer::singleShot(0,&page,[&page]{
            auto box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());QVERIFY(box);
            page.findChild<QTableWidget*>("locationTableWidget")->setCurrentCell(1,0);
            box->button(QMessageBox::Yes)->click();
        });
        QVERIFY(QMetaObject::invokeMethod(&page,"removeLocation",Qt::DirectConnection));
        QCOMPARE(directory.networkObjects().size(),1);QCOMPARE(NetworkObject(directory.networkObjects()[0].toObject()).uid(),other);
    }
    void consolePreviewUsesActualButtonsAndCopyrightMenu() {
        QMainWindow window;window.resize(1120,680);window.setWindowTitle(NetLinkUi::windowTitle("NetLink — anteprima Console docente"));
        NetLinkUi::addInformationMenu(&window);NetLinkUi::addInformationMenu(&window);
        QCOMPARE(window.findChildren<QMenu*>("netlinkInformationMenu").size(),1);
        auto bar=window.addToolBar("Comandi");bar->setMovable(false);
        bar->setIconSize(QSize(36,36));
        for(const auto& pair:QList<QPair<QString,QString>>{{"MonitoringMode","Panoramica"},{"Demo","Dimostrazione"},{"RemoteControl","Controllo"},{"ScreenLock","Blocca schermo"},{"InternetGuard","InternetGuard"},{"Appello","Appello"},{"Discovery","Trova client"},{"TextMessage","Messaggio"}}) {
            Feature feature(pair.first,Feature::Flag::Action,QUuid::createUuid(),{},pair.second,{},"Descrizione");
            auto button=new ToolButton(NetLinkUi::icon(pair.first,"Organic",QColor("#0f766e")),pair.second,{},NetLinkUi::featureDescription(feature));
            button->addTo(bar);
            button->setIconSize(QSize(36,36));
        }
        auto central=new QWidget(&window);auto root=new QVBoxLayout(central);window.setCentralWidget(central);
        auto note=new QLabel("Anteprima grafica — dati dimostrativi, nessun PC collegato",central);root->addWidget(note);
        auto grid=new QGridLayout;root->addLayout(grid,1);
        int i=0;for(const auto& name:{"Anna Rossi","Luca Bianchi","Sara Verdi","Marco Neri"}) {
            auto card=new QGroupBox(QString("PC-0%1 • %2").arg(i+1).arg(name),central);auto layout=new QVBoxLayout(card);
            auto screen=new QLabel("Desktop studente\n\nMiniatura del client",card);screen->setAlignment(Qt::AlignCenter);
            screen->setStyleSheet("QLabel{background:#d9ede8;color:#194d43;border-radius:8px;padding:35px;}");layout->addWidget(screen,1);
            grid->addWidget(card,i/2,i%2);++i;
        }
        QVERIFY(window.findChildren<QLabel*>("netlinkCopyright").isEmpty());
        window.show();QApplication::processEvents();snapshot(window,"netlink-console");
    }
    void trayDetailsCannotQuitWorkerOrCreateDuplicateWindows() {
        QPointer<QMessageBox> box=NetLinkUi::showTrayDetails("TCP 11100: in ascolto","test","12:00:00");
        QVERIFY(box);QVERIFY(!box->testAttribute(Qt::WA_QuitOnClose));QCOMPARE(box->textFormat(),Qt::PlainText);
        QCOMPARE(NetLinkUi::showTrayDetails("Seconda istantanea\n\nClient: 192.0.2.11","test","12:00:15"),box.data());
        QVERIFY(box->detailedText().contains("192.0.2.11"));QVERIFY(!box->text().contains("192.0.2.11"));
        QVERIFY(box->windowTitle().contains(NetLinkUi::copyrightText()));
        QVERIFY(!box->text().contains(NetLinkUi::copyrightText()));QVERIFY(box->text().contains("Seconda istantanea"));
        snapshot(*box,"netlink-stato-servizio");box->close();QTRY_VERIFY(box.isNull());
    }
};
int main(int argc,char** argv) {
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    QApplication app(argc,argv);NetLinkUiTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "NetLinkUiTests.moc"
