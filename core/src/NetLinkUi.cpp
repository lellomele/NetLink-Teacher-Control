// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include "NetLinkUi.h"
#include "AboutDialog.h"
#include "Feature.h"
#include "NetLinkAppearance.h"
#include <QApplication>
#include <QGroupBox>
#include <QIconEngine>
#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QSet>
#include <cmath>

namespace {
QString glyph(const QString& name) {
    if(name.contains("Demo") || name=="Presentation") return "demo";
    if(name.contains("PowerDown")) return "power";
    if(name=="MonitoringMode") return "monitor";
    if(name=="InternetGuard") return "shield";
    if(name=="Appello") return "people";
    if(name=="Discovery") return "search";
    if(name=="RemoteView") return "eye";
    if(name=="RemoteControl") return "pointer";
    if(name=="ScreenLock" || name=="InputDevicesLock") return "lock";
    if(name=="TextMessage") return "message";
    if(name=="Screenshot") return "camera";
    if(name=="PowerOn") return "power";
    if(name=="Reboot") return "restart";
    if(name=="StartApp") return "app";
    if(name=="OpenWebsite") return "globe";
    if(name=="ClipboardExchange") return "clipboard";
    if(name=="FileTransfer") return "files";
    if(name=="UserLogon" || name=="UserLogin") return "login";
    if(name=="UserLogoff") return "logout";
    if(QStringList{"add","remove","settings","info","key","group","monitor","pin","keyboard","fullscreen"}.contains(name)) return name;
    return {};
}
class VectorIcon final : public QIconEngine {
public:
    VectorIcon(QString symbol, bool soft, QColor color, bool colorful=false) : m_symbol(std::move(symbol)),m_soft(soft),m_color(color),m_colorful(colorful) {}
    QIconEngine* clone() const override { return new VectorIcon(*this); }
    QString key() const override { return "NetLinkVectorIcon"; }
    QSize actualSize(const QSize& size,QIcon::Mode,QIcon::State) override { return size; }
    QPixmap pixmap(const QSize& size,QIcon::Mode mode,QIcon::State state) override {
        QPixmap result(size);result.fill(Qt::transparent);QPainter painter(&result);
        paint(&painter,QRect(QPoint(),size),mode,state);return result;
    }
    void paint(QPainter* painter,const QRect& rect,QIcon::Mode mode,QIcon::State) override {
        painter->save();painter->setRenderHint(QPainter::Antialiasing);
        const auto side=qMin(rect.width(),rect.height());
        painter->translate(rect.center().x()-side/2.,rect.center().y()-side/2.);
        painter->scale(side/32.,side/32.);
        QColor color=m_color;
        if(mode==QIcon::Disabled) color=qApp->palette().color(QPalette::Disabled,QPalette::ButtonText);
        if(mode==QIcon::Selected) color=qApp->palette().color(QPalette::HighlightedText);
        if(m_soft) {
            auto tint=color;tint.setAlpha(m_colorful?52:28);painter->setPen(Qt::NoPen);painter->setBrush(tint);
            QPainterPath blob;blob.moveTo(6,2);blob.cubicTo(19,-1,32,4,30,18);
            blob.cubicTo(29,30,19,33,8,29);blob.cubicTo(-1,26,-1,8,6,2);painter->drawPath(blob);
        }
        painter->setPen(QPen(color,1.8,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        painter->setBrush(Qt::NoBrush);
        const auto line=[&](qreal a,qreal b,qreal c,qreal d){painter->drawLine(QPointF(a,b),QPointF(c,d));};
        const auto box=[&](qreal x,qreal y,qreal w,qreal h,qreal r=2.){painter->drawRoundedRect(QRectF(x,y,w,h),r,r);};
        const auto circle=[&](qreal x,qreal y,qreal r){painter->drawEllipse(QPointF(x,y),r,r);};
        if(m_symbol=="shield") {
            QPainterPath p;p.moveTo(16,5);p.lineTo(25,9);p.lineTo(24,18);p.cubicTo(23,23,19,26,16,28);
            p.cubicTo(13,26,9,23,8,18);p.lineTo(7,9);p.closeSubpath();painter->drawPath(p);
            if(m_colorful && mode==QIcon::Normal) painter->setPen(QPen(QColor(qApp->palette().color(QPalette::Button).lightness()<128?"#f1bd52":"#9c650e"),2.2,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
            line(11,16,15,20);line(15,20,22,12);
        } else if(m_symbol=="people" || m_symbol=="group") {
            circle(13,11,4);circle(23,13,3);QPainterPath p;p.moveTo(5,26);p.cubicTo(5,15,21,15,21,26);
            p.moveTo(22,20);p.cubicTo(26,20,28,22,28,26);painter->drawPath(p);
        } else if(m_symbol=="search") {circle(14,14,8);line(20,20,27,27);line(10,14,18,14);line(14,10,14,18);
        } else if(m_symbol=="eye") {
            QPainterPath p;p.moveTo(4,16);p.cubicTo(11,5,21,5,28,16);p.cubicTo(21,27,11,27,4,16);painter->drawPath(p);circle(16,16,4);
        } else if(m_symbol=="pointer") {
            QPainterPath p;p.moveTo(8,5);p.lineTo(9,25);p.lineTo(14,20);p.lineTo(18,27);p.lineTo(22,25);p.lineTo(18,18);p.lineTo(25,17);p.closeSubpath();painter->drawPath(p);
        } else if(m_symbol=="lock") {box(7,14,18,13);painter->drawArc(QRectF(11,5,10,18),0,180*16);circle(16,20,1);line(16,21,16,23);
        } else if(m_symbol=="message") {box(5,6,22,17,4);line(10,23,9,28);line(9,28,16,23);line(10,12,22,12);line(10,17,19,17);
        } else if(m_symbol=="camera") {box(4,10,24,16);box(11,6,10,4);circle(16,18,5);
        } else if(m_symbol=="power") {painter->drawArc(QRectF(6,6,20,21),125*16,290*16);line(16,4,16,16);
        } else if(m_symbol=="restart") {painter->drawArc(QRectF(6,6,20,20),35*16,290*16);line(25,5,25,12);line(25,12,18,12);
        } else if(m_symbol=="globe") {circle(16,16,11);painter->drawEllipse(QRectF(11,5,10,22));line(5,16,27,16);line(8,10,24,10);line(8,22,24,22);
        } else if(m_symbol=="app" || m_symbol=="files") {box(5,6,22,21);line(5,12,27,12);circle(9,9,0.5);circle(13,9,0.5);line(12,17,20,17);line(12,21,18,21);
        } else if(m_symbol=="login" || m_symbol=="logout") {box(7,5,17,23);line(4,16,19,16);line(15,12,19,16);line(19,16,15,20);
        } else if(m_symbol=="key") {circle(11,12,6);line(15,17,26,28);line(21,23,24,20);line(24,26,27,23);
        } else if(m_symbol=="add" || m_symbol=="remove") {line(7,16,25,16);if(m_symbol=="add") line(16,7,16,25);
        } else if(m_symbol=="info") {circle(16,16,11);circle(16,10,0.7);line(16,15,16,23);
        } else if(m_symbol=="settings") {circle(16,16,8);circle(16,16,3);for(int i=0;i<8;++i){painter->save();painter->translate(16,16);painter->rotate(i*45);line(0,10,0,13);painter->restore();}
        } else if(m_symbol=="pin") {
            line(12,5,24,5);line(14,5,14,13);line(22,5,22,13);line(14,13,9,19);line(22,13,27,19);
            line(9,19,27,19);line(18,19,18,28);
        } else if(m_symbol=="keyboard") {
            box(3,8,26,17,3);for(int y=12;y<=16;y+=4) for(int x=7;x<=25;x+=6) line(x,y,x+1,y);
            line(9,21,23,21);
        } else if(m_symbol=="fullscreen") {
            line(5,12,5,5);line(5,5,12,5);line(20,5,27,5);line(27,5,27,12);
            line(5,20,5,27);line(5,27,12,27);line(20,27,27,27);line(27,27,27,20);
        } else if(m_symbol=="clipboard") {box(8,7,17,21);box(12,4,9,5);line(12,15,21,15);line(12,20,21,20);
        } else {box(5,6,22,17);line(16,23,16,27);line(10,27,22,27);if(m_symbol=="demo"){line(14,10,21,15);line(21,15,14,20);line(14,20,14,10);}else{line(10,12,13,12);line(19,12,22,12);line(10,17,13,17);line(19,17,22,17);}}
        painter->restore();
    }
private: QString m_symbol;bool m_soft;QColor m_color;bool m_colorful;
};
}

QString NetLinkUi::copyrightText() { return QString::fromUtf8("© 2026 prof. Ing. Raffaele Mele"); }
QString NetLinkUi::displayVersion(const QString& technicalVersion) {
    const auto marker=QStringLiteral("-netlink.");
    const auto position=technicalVersion.indexOf(marker);
    return position>=0 && position+marker.size()<technicalVersion.size() ? technicalVersion.mid(position+marker.size()) : technicalVersion;
}
QString NetLinkUi::applicationName(const QString& configuredName) {
    const auto name=configuredName.trimmed();
    return name.isEmpty() || name.compare("Veyon",Qt::CaseInsensitive)==0 ||
        name.compare("Veyon School",Qt::CaseInsensitive)==0 ? QStringLiteral("NetLink") : name;
}
QString NetLinkUi::windowTitle(const QString& title) {
    return title;
}
QDialog* NetLinkUi::showAbout(QWidget* parent) {
    for(auto widget:QApplication::topLevelWidgets()) {
        if(widget->objectName()=="netlinkAboutDialog" && widget->parentWidget()==parent) {
            widget->show();widget->raise();widget->activateWindow();return qobject_cast<QDialog*>(widget);
        }
    }
    auto dialog=new AboutDialog(parent);dialog->setObjectName("netlinkAboutDialog");
    dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setAttribute(Qt::WA_QuitOnClose,false);
    dialog->setModal(false);
    dialog->show();return dialog;
}
QIcon NetLinkUi::icon(const QString& name,const QString& set,const QColor& accent,const QIcon& fallback) {
    const auto symbol=glyph(name);
    if(symbol.isEmpty() || (set=="Classic" && !fallback.isNull())) return fallback;
    // Respect the active accessibility palette instead of a user accent in high contrast.
    const bool colorful=set=="Spectrum" && !qApp->property("netlinkHighContrast").toBool();
    static const QHash<QString,QColor> colors{{"monitor",QColor("#258dde")},{"demo",QColor("#8661d1")},
        {"pointer",QColor("#2b8ce0")},{"eye",QColor("#a45ac5")},{"shield",QColor("#17a885")},
        {"people",QColor("#e18432")},{"group",QColor("#e18432")},{"search",QColor("#28a7bd")},
        {"lock",QColor("#db6267")},{"power",QColor("#dc6651")},{"restart",QColor("#dc9a29")},
        {"message",QColor("#2da2b4")},{"app",QColor("#657bde")},{"files",QColor("#cf9b24")},
        {"globe",QColor("#3598d3")},{"camera",QColor("#a96bcb")},{"login",QColor("#2ca47c")},
        {"logout",QColor("#d28d2b")},{"clipboard",QColor("#8d79cb")}};
    auto color=qApp->property("netlinkHighContrast").toBool()?qApp->palette().color(QPalette::ButtonText):
        colorful?colors.value(symbol,QColor("#258dde")):accent;
    if(colorful && name=="PowerOn") color=QColor("#2ca47c");
    const auto luminance=[](const QColor& value){
        const auto linear=[](double v){return v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);};
        return .2126*linear(value.redF())+.7152*linear(value.greenF())+.0722*linear(value.blueF());
    };
    const auto a=luminance(color),b=luminance(qApp->palette().color(QPalette::Button));
    if((qMax(a,b)+.05)/(qMin(a,b)+.05)<3.) {
        if(colorful) for(int i=0;i<12 && (qMax(luminance(color),b)+.05)/(qMin(luminance(color),b)+.05)<3.;++i)
            color=b<.2?color.lighter(110):color.darker(110);
        else color=qApp->palette().color(QPalette::ButtonText);
    }
    return QIcon(new VectorIcon(symbol,set!="Outline",color,colorful));
}
QString NetLinkUi::featureDescription(const Feature& feature) {
    const auto name=feature.name();
    if(name=="InternetGuard") return NetLinkUi::text("Opens Internet controls for the selected computers. Apply or remove filters and read each client's feedback. Closing the panel does not remove the block.");
    if(name=="Appello") return NetLinkUi::text("Asks students on the selected computers to enter their names and displays them on thumbnails. Names are self-declared: they are not Windows account names or verified identities.");
    if(name=="Discovery") return NetLinkUi::text("Finds remote clients using WS-Discovery and shows IPv4 addresses. Verify computers before importing: an announcement does not authenticate a client. Across subnets, configure the teacher's name/IP on the client.");
    if(name=="MonitoringMode") return NetLinkUi::text("Shows updated thumbnails of configured, selected computers without sending control commands. If a thumbnail cannot connect, check its status and use Companion.");
    if(name=="RemoteControl") return NetLinkUi::text("Opens the selected computer's desktop with mouse and keyboard control. Requires authentication and client authorization. Close the window to end the session.");
    if(name=="RemoteView") return NetLinkUi::text("Opens the selected computer's desktop for viewing only, without sending mouse or keyboard input. Requires authentication and client authorization.");
    if(name=="ScreenLock") return NetLinkUi::text("Applies or removes the software screen cover on selected computers. Ordinary shortcuts are filtered during lock. Windows security screens remain accessible: their exact triggering shortcut cannot be identified. Lock events require updated NetLink on both PCs.");
    if(name=="PowerOn") return NetLinkUi::text("Sends Wake-on-LAN to selected computers. Requires a configured MAC, compatible network adapter and BIOS. Wake-up over Wi-Fi is not guaranteed.");
    if(name.contains("PowerDown")) return NetLinkUi::text("Shuts down selected computers. This can interrupt students' work: verify recipients and save documents before confirming.");
    if(name=="Reboot") return NetLinkUi::text("Restarts selected computers. This can interrupt students' work: verify recipients and save documents before confirming.");
    if(name=="TextMessage") return NetLinkUi::text("Sends a message to students on selected computers. It is displayed on the client; this is not a persistent chat.");
    if(name=="Screenshot") return NetLinkUi::text("Saves a desktop image from selected computers. Screenshots may contain personal data: store and share them carefully.");
    if(name.contains("Demo")) return feature.description()+NetLinkUi::text("\nShares a screen with demonstration recipients. Check selected computers and end the mode when finished.");
    return feature.description()+NetLinkUi::text("\nOperates on selected computers in the Console. A disconnected computer cannot confirm execution.");
}
void NetLinkUi::addInformationMenu(QMainWindow* window) {
    if(window->findChild<QMenu*>("netlinkInformationMenu")) return;
    auto menu=window->menuBar()->addMenu("NetLink");menu->setObjectName("netlinkInformationMenu");
    menu->setToolTipsVisible(true);
    auto appearance=menu->addAction(NetLinkUi::text("Appearance, colors and icons…"));appearance->setObjectName("netlinkAppearanceAction");
    appearance->setToolTip(NetLinkUi::text("Choose a theme, colors, icon set and button size. These are personal preferences and do not change keys or the service."));
    QObject::connect(appearance,&QAction::triggered,window,[window]{NetLinkAppearance::install()->showSettings(window);});
    auto about=menu->addAction(NetLinkUi::text("About NetLink"));about->setObjectName("netlinkAboutAction");
    QObject::connect(about,&QAction::triggered,window,[window]{showAbout(window);});
}
bool NetLinkUi::easyPage(const QString& name) {
    return QStringList{"GeneralConfigurationPage","ServiceConfigurationPage","AuthKeysConfigurationPage",
        "BuiltinDirectoryConfigurationPage","SchoolConfigurationPage"}.contains(name);
}
void NetLinkUi::setEasyPage(QWidget* page,bool easy) {
    const auto name=QString::fromLatin1(page->metaObject()->className());
    const auto groups=page->findChildren<QGroupBox*>(QString(),Qt::FindDirectChildrenOnly);
    for(auto group:groups) {
        const bool visible=!easy || (name=="GeneralConfigurationPage"?group->objectName()=="groupBox":
            name=="ServiceConfigurationPage"?group->objectName()=="groupBox_3":true);
        // Standard/advanced visibility is applied by Configurator before this helper.
        if(easy) group->setVisible(visible);
        else if(group->property("netlinkHiddenByEasy").toBool()) group->show();
        group->setProperty("netlinkHiddenByEasy",easy && !visible);
    }
    for(auto widget:page->findChildren<QWidget*>()) if(widget->property("netlinkEasyHidden").toBool()) widget->setVisible(!easy);
}
QString NetLinkUi::traySummary(const QString& host,int port,bool listening,bool localOnly,
    bool keyAuthentication,bool discovery,int authenticated,int pending) {
    QString safeHost;
    for(const auto c:host) if(c.category()!=QChar::Other_Control && c.category()!=QChar::Other_Format) safeHost+=c;
    safeHost=safeHost.simplified().left(18);
    // Windows' native tray tooltip is short. Full version/meaning is in its menu.
    return QString(NetLinkUi::text("NetLink Service — %1\nTCP %2: %3\n%4 • WSD %5\nAuthorized %6 • other TCP %7"))
        .arg(safeHost,QString::number(port),!listening?NetLinkUi::text("not listening"):localOnly?NetLinkUi::text("local only"):NetLinkUi::text("listening"),
             keyAuthentication?NetLinkUi::text("Key"):"Account",discovery?NetLinkUi::text("configured"):NetLinkUi::text("off"),QString::number(qBound(0,authenticated,9999)),QString::number(qBound(0,pending,9999))).left(127);
}
QMessageBox* NetLinkUi::showTrayDetails(const QString& snapshot,const QString& version,const QString& updated) {
    QMessageBox* box=nullptr;
    for(auto widget:QApplication::topLevelWidgets()) if(widget->objectName()=="netlinkTrayDetails") box=qobject_cast<QMessageBox*>(widget);
    if(!box) {
        box=new QMessageBox(QMessageBox::Information,windowTitle(NetLinkUi::text("NetLink — local status")),{},QMessageBox::Ok);
        box->setObjectName("netlinkTrayDetails");box->setTextFormat(Qt::PlainText);
        box->setAttribute(Qt::WA_DeleteOnClose);box->setAttribute(Qt::WA_QuitOnClose,false);
        box->setWindowIcon(QIcon(":/core/veyon.png"));box->setTextInteractionFlags(Qt::TextSelectableByMouse);
    }
    box->setText(snapshot.section("\n\n",0,0).left(512)+NetLinkUi::text("\n\nVersion: ")+displayVersion(version).left(100)+NetLinkUi::text("\nLast received: ")+updated.left(32));
    box->setInformativeText(NetLinkUi::text("Local server snapshot, not a network test. Use Companion to diagnose connection problems."));
    box->setDetailedText(snapshot.section("\n\n",1).left(1536)+NetLinkUi::text("\n\nAuthorized = connections with successful authentication and access control. Other TCP = connections not yet authorized.\nWSD = WS-Discovery configuration, not successful discovery. Listening does not prove reachability from another computer.\nThis does not verify Internet blocking. Use Companion for network, firewall and key diagnostics."));
    box->show();box->raise();box->activateWindow();return box;
}
