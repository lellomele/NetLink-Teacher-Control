// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include <QtTest>
#include <QApplication>
#include <QAbstractItemView>
#include <QComboBox>
#include <QDir>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QScreen>
#include <QScrollBar>
#include <QSettings>
#include <QSignalSpy>
#include <QStyleOptionComboBox>
#include <QTemporaryDir>
#include <QVBoxLayout>
#include <memory>
#include "NetLinkAppearance.h"

// Real Qt popup windows and the shared production appearance manager. No
// VeyonCore initialization, registry, authentication keys or network activity.
class ComboPopupTests : public QObject {
    Q_OBJECT
    void snapshot(QWidget& host, QComboBox& combo, const QString& name) {
        const auto output=qEnvironmentVariable("NETLINK_COMBO_SCREENSHOTS");
        if(output.isEmpty()) return;
        QVERIFY(QDir().mkpath(output));
        const auto prefix=name+QString("-%1").arg(qRound(host.devicePixelRatioF()*100));
        QVERIFY(host.grab().save(QDir(output).filePath(prefix+"-host.png")));
        QVERIFY(combo.view()->window()->grab().save(QDir(output).filePath(prefix+"-popup.png")));
    }
private Q_SLOTS:
    void cleanup() { QApplication::setFont(QFont("Segoe UI",10)); }
    void popupReadability_data() {
        QTest::addColumn<QString>("mode");
        QTest::addColumn<QString>("context");
        QTest::addColumn<QStringList>("items");
        QTest::addColumn<bool>("editable");
        QTest::addColumn<int>("fontSize");
        for(const auto& mode:{QString("Light"),QString("Dark")}) for(int font:{10,14}) {
            const auto prefix=mode+QString::number(font);
            QTest::newRow(qPrintable(prefix+"-Configurator"))<<mode<<QString("Authentication")
                <<QStringList{"Logon authentication","Key file authentication"}<<false<<font;
            QTest::newRow(qPrintable(prefix+"-Master"))<<mode<<QString("Discovery")
                <<QStringList{"Automatic — prefer IPv4","IPv4 only"}<<false<<font;
            QTest::newRow(qPrintable(prefix+"-Companion"))<<mode<<QString("Companion")
                <<QStringList{"Scegli il ruolo","PC docente / Master","PC studente / client"}<<false<<font;
            QTest::newRow(qPrintable(prefix+"-Editable"))<<mode<<QString("Companion target")
                <<QStringList{"client01.example","192.0.2.20","2001:db8::20"}<<true<<font;
        }
    }
    void popupReadability() {
        QFETCH(QString,mode);QFETCH(QString,context);QFETCH(QStringList,items);QFETCH(bool,editable);QFETCH(int,fontSize);
        QApplication::setFont(QFont("Segoe UI",fontSize));
        QTemporaryDir temp;QVERIFY(temp.isValid());QSettings settings(temp.filePath("appearance.ini"),QSettings::IniFormat);
        NetLinkAppearance::Preferences preferences;preferences.mode=mode;
        QVERIFY(NetLinkAppearance::save(settings,preferences));
        std::unique_ptr<NetLinkAppearance> appearance(NetLinkAppearance::install(&settings));
        QWidget host;host.setWindowTitle("NetLink — "+context);host.setFont(QFont("Segoe UI",fontSize));
        auto layout=new QVBoxLayout(&host);auto group=new QGroupBox(context,&host);layout->addWidget(group);
        auto form=new QVBoxLayout(group);form->addWidget(new QLabel("Choose an option / Scegli un'opzione",group));
        QComboBox combo(group);combo.setFont(QFont("Segoe UI",fontSize));combo.setEditable(editable);combo.addItems(items);form->addWidget(&combo);
        const auto screen=QGuiApplication::primaryScreen()->availableGeometry();
        host.resize(qMin(520,screen.width()-24),150);host.move(screen.topLeft()+QPoint(12,12));host.show();
        QTest::qWait(30);combo.showPopup();QTRY_VERIFY(combo.view()->isVisible());
        auto view=combo.view();view->doItemsLayout();
        QCOMPARE(view->font().pointSize(),fontSize);
        snapshot(host,combo,mode+"-"+context+QString::number(fontSize));
        QStyleOptionComboBox option;option.initFrom(&combo);option.editable=editable;
        QCOMPARE(combo.style()->styleHint(QStyle::SH_ComboBox_Popup,&option,&combo),0);
        for(int row=0;row<items.size();++row) {
            const auto rect=view->visualRect(combo.model()->index(row,combo.modelColumn()));
            QVERIFY2(rect.height()>=view->fontMetrics().height(),qPrintable(QString("Row %1 clips font height").arg(row)));
            QVERIFY2(view->viewport()->rect().contains(rect),qPrintable(QString("Row %1 not fully visible: %2,%3 %4x%5 in %6x%7")
                .arg(row).arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height()).arg(view->viewport()->width()).arg(view->viewport()->height())));
            QVERIFY2(rect.width()>=view->fontMetrics().horizontalAdvance(items[row]),"Popup clips option width");
        }
        // Esc must not silently change an authentication or network option.
        QSignalSpy changed(&combo,QOverload<int>::of(&QComboBox::currentIndexChanged));
        QTest::keyClick(view,Qt::Key_Escape);QTRY_VERIFY(!view->isVisible());
        QCOMPARE(combo.currentIndex(),0);QCOMPARE(changed.count(),0);
        combo.showPopup();QTRY_VERIFY(view->isVisible());
        QTest::keyClick(view,Qt::Key_End);QTest::keyClick(view,Qt::Key_Return);
        QTRY_VERIFY(!view->isVisible());QCOMPARE(combo.currentIndex(),items.size()-1);
    }
    void longListAndScreenEdge() {
        QTemporaryDir temp;QVERIFY(temp.isValid());QSettings settings(temp.filePath("long.ini"),QSettings::IniFormat);
        std::unique_ptr<NetLinkAppearance> appearance(NetLinkAppearance::install(&settings));
        QWidget host;auto layout=new QVBoxLayout(&host);QComboBox combo(&host);layout->addWidget(&combo);
        for(int i=0;i<60;++i) combo.addItem(QString("Client %1 — classroom network").arg(i,2,10,QChar('0')));
        const auto screen=QGuiApplication::primaryScreen()->availableGeometry();
        host.resize(qMin(400,screen.width()-24),70);
        host.move(screen.left()+12,screen.bottom()-host.height()-20);host.show();QTest::qWait(30);
        combo.showPopup();auto view=combo.view();QTRY_VERIFY(view->isVisible());
        QVERIFY(view->verticalScrollBar()->maximum()>0);
        QVERIFY(screen.contains(view->window()->frameGeometry()));
        view->scrollToBottom();view->doItemsLayout();
        QTRY_VERIFY(view->viewport()->rect().contains(view->visualRect(combo.model()->index(59,0))));
        snapshot(host,combo,"long-list-bottom-edge");
        combo.hidePopup();
    }
};
int main(int argc,char** argv) {
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication app(argc,argv);QApplication::setFont(QFont("Segoe UI",10));
    ComboPopupTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "ComboPopupTests.moc"
