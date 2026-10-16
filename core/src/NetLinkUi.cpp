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
    if(QStringList{"add","remove","settings","info","key","group","monitor"}.contains(name)) return name;
    return {};
}
class VectorIcon final : public QIconEngine {
public:
    VectorIcon(QString symbol, bool soft, QColor color) : m_symbol(std::move(symbol)),m_soft(soft),m_color(color) {}
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
            auto tint=color;tint.setAlpha(28);painter->setPen(Qt::NoPen);painter->setBrush(tint);
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
        } else if(m_symbol=="app") {box(5,6,22,21);line(5,12,27,12);circle(9,9,0.5);circle(13,9,0.5);line(12,17,20,17);line(12,21,18,21);
        } else if(m_symbol=="key") {circle(11,12,6);line(15,17,26,28);line(21,23,24,20);line(24,26,27,23);
        } else if(m_symbol=="add" || m_symbol=="remove") {line(7,16,25,16);if(m_symbol=="add") line(16,7,16,25);
        } else if(m_symbol=="info") {circle(16,16,11);circle(16,10,0.7);line(16,15,16,23);
        } else if(m_symbol=="settings") {circle(16,16,8);circle(16,16,3);for(int i=0;i<8;++i){painter->save();painter->translate(16,16);painter->rotate(i*45);line(0,10,0,13);painter->restore();}
        } else if(m_symbol=="clipboard") {box(8,7,17,21);box(12,4,9,5);line(12,15,21,15);line(12,20,21,20);
        } else {box(5,6,22,17);line(16,23,16,27);line(10,27,22,27);if(m_symbol=="demo"){line(14,10,21,15);line(21,15,14,20);line(14,20,14,10);}else{line(10,12,13,12);line(19,12,22,12);line(10,17,13,17);line(19,17,22,17);}}
        painter->restore();
    }
private: QString m_symbol;bool m_soft;QColor m_color;
};
}

QString NetLinkUi::copyrightText() { return QString::fromUtf8("© 2026 prof. Ing. Raffaele Mele"); }
QString NetLinkUi::windowTitle(const QString& title) {
    return title.contains(copyrightText()) ? title : title+QString::fromUtf8(" — ")+copyrightText();
}
QIcon NetLinkUi::icon(const QString& name,const QString& set,const QColor& accent,const QIcon& fallback) {
    const auto symbol=glyph(name);
    if(symbol.isEmpty() || (set=="Classic" && !fallback.isNull())) return fallback;
    // Respect the active accessibility palette instead of a user accent in high contrast.
    auto color=qApp->property("netlinkHighContrast").toBool()?qApp->palette().color(QPalette::ButtonText):accent;
    const auto luminance=[](const QColor& value){
        const auto linear=[](double v){return v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);};
        return .2126*linear(value.redF())+.7152*linear(value.greenF())+.0722*linear(value.blueF());
    };
    const auto a=luminance(color),b=luminance(qApp->palette().color(QPalette::Button));
    if((qMax(a,b)+.05)/(qMin(a,b)+.05)<3.) color=qApp->palette().color(QPalette::ButtonText);
    return QIcon(new VectorIcon(symbol,set!="Outline",color));
}
QString NetLinkUi::featureDescription(const Feature& feature) {
    const auto name=feature.name();
    if(name=="InternetGuard") return "Apre il controllo Internet dei PC selezionati. Puoi attivare o rimuovere i filtri e leggere il riscontro di ogni client. Chiudere il pannello non rimuove il blocco.";
    if(name=="Appello") return "Chiede agli studenti dei PC selezionati di inserire il proprio nome e lo mostra sulle miniature. Il nome è dichiarato dallo studente: non identifica l’account Windows e non verifica l’identità.";
    if(name=="Discovery") return "Cerca client remoti tramite WS-Discovery e mostra indirizzi IPv4. Verifica i PC prima di importarli: un annuncio non autentica il client. Tra subnet può servire il nome/IP del docente configurato sul client.";
    if(name=="MonitoringMode") return "Mostra le miniature aggiornate dei PC configurati e selezionati. Non invia comandi di controllo agli studenti. Se una miniatura non si collega, consulta lo stato e Companion.";
    if(name=="RemoteControl") return "Apre il desktop del PC selezionato e consente di usare mouse e tastiera. Richiede autenticazione e autorizzazione del client. Chiudi la finestra per terminare la sessione.";
    if(name=="RemoteView") return "Apre il desktop del PC selezionato in sola visualizzazione, senza inviare mouse o tastiera. Richiede autenticazione e autorizzazione del client.";
    if(name=="ScreenLock") return "Attiva o rimuove il blocco schermo sui PC selezionati. Controlla lo stato dei client prima di considerare il blocco applicato.";
    if(name=="PowerOn") return "Invia Wake-on-LAN ai PC selezionati. Richiede MAC configurato, scheda di rete e BIOS compatibili; non garantisce l’accensione tramite Wi-Fi.";
    if(name.contains("PowerDown")) return "Invia il comando di spegnimento ai PC selezionati. Può interrompere il lavoro degli studenti: verifica i destinatari e salva i documenti prima di confermare.";
    if(name=="Reboot") return "Riavvia i PC selezionati. Può interrompere il lavoro degli studenti: verifica i destinatari e salva i documenti prima di confermare.";
    if(name=="TextMessage") return "Invia un messaggio agli studenti dei PC selezionati. Il testo viene mostrato sul client; non è una chat permanente.";
    if(name=="Screenshot") return "Salva un’immagine del desktop dei PC selezionati. Le schermate possono contenere dati personali: conserva e condividi i file con attenzione.";
    if(name.contains("Demo")) return feature.description()+"\nCondivide uno schermo con i destinatari della dimostrazione. Verifica i PC selezionati e termina la modalità quando hai finito.";
    return feature.description()+"\nOpera sui PC selezionati nella Console; un computer non connesso non conferma l’esecuzione del comando.";
}
void NetLinkUi::addInformationMenu(QMainWindow* window) {
    if(window->findChild<QMenu*>("netlinkInformationMenu")) return;
    auto menu=window->menuBar()->addMenu("NetLink");menu->setObjectName("netlinkInformationMenu");
    menu->setToolTipsVisible(true);
    auto appearance=menu->addAction("Aspetto, colori e icone…");appearance->setObjectName("netlinkAppearanceAction");
    appearance->setToolTip("Scegli tema, colori, set di icone e dimensione dei pulsanti. Sono preferenze personali, non modificano chiavi o servizio.");
    QObject::connect(appearance,&QAction::triggered,window,[window]{NetLinkAppearance::install()->showSettings(window);});
    auto about=menu->addAction("Informazioni, copyright e licenze…");about->setObjectName("netlinkAboutAction");
    QObject::connect(about,&QAction::triggered,window,[window]{
        if(auto previous=window->findChild<AboutDialog*>("netlinkAboutDialog")){previous->show();previous->raise();previous->activateWindow();return;}
        auto dialog=new AboutDialog(window);dialog->setObjectName("netlinkAboutDialog");dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->open();
    });
    menu->addSeparator();auto copyright=menu->addAction(copyrightText());copyright->setEnabled(false);
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
    return QString("NetLink Service — %1\nTCP %2: %3\n%4 • WSD %5\nAccessi %6 • altri TCP %7")
        .arg(safeHost,QString::number(port),!listening?"non in ascolto":localOnly?"solo locale":"in ascolto",
             keyAuthentication?"Chiave":"Account",discovery?"config.":"spento",QString::number(qBound(0,authenticated,9999)),QString::number(qBound(0,pending,9999))).left(127);
}
QMessageBox* NetLinkUi::showTrayDetails(const QString& snapshot,const QString& version,const QString& updated) {
    QMessageBox* box=nullptr;
    for(auto widget:QApplication::topLevelWidgets()) if(widget->objectName()=="netlinkTrayDetails") box=qobject_cast<QMessageBox*>(widget);
    if(!box) {
        box=new QMessageBox(QMessageBox::Information,windowTitle("NetLink — stato locale"),{},QMessageBox::Ok);
        box->setObjectName("netlinkTrayDetails");box->setTextFormat(Qt::PlainText);
        box->setAttribute(Qt::WA_DeleteOnClose);box->setAttribute(Qt::WA_QuitOnClose,false);
        box->setWindowIcon(QIcon(":/core/veyon.png"));box->setTextInteractionFlags(Qt::TextSelectableByMouse);
    }
    box->setText(snapshot.section("\n\n",0,0).left(512)+"\n\nVersione: "+version.left(100)+"\nUltimi dati ricevuti: "+updated.left(32));
    box->setInformativeText("Istantanea del server locale, non un test di rete. Per problemi di collegamento usa Companion.");
    box->setDetailedText(snapshot.section("\n\n",1).left(1536)+"\n\nAccessi = connessioni con autenticazione e autorizzazione riuscite. Altri TCP = connessioni non ancora autorizzate.\n"
        "WSD = WS-Discovery: indica la configurazione, non la riuscita della ricerca. In ascolto non prova la raggiungibilità da altri PC.\n"
        "Questa non è una verifica del blocco Internet. Per rete, firewall e chiavi usa Companion.");
    box->show();box->raise();box->activateWindow();return box;
}
