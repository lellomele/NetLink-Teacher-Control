// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025-26 prof. Ing. Raffaele Mele
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include "Diagnostics.h"
#include "DiagnosticLink.h"
#include "NetLinkAppearance.h"
#include <veyonconfig.h>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSpinBox>
#include <QTcpSocket>
#include <QTextBrowser>
#include <QPlainTextEdit>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif
#include <cstdio>

class CompanionWindow : public QMainWindow {
public:
    explicit CompanionWindow(const QJsonObject& initialReport={},bool autoScan=true) {
        setWindowTitle("NetLink Companion — diagnosi e configurazione assistita");
        setWindowIcon(QIcon(":/companion/icon.png"));resize(1050,800);
        auto central=new QWidget(this);setCentralWidget(central);auto root=new QVBoxLayout(central);
        auto tabs=new QTabWidget(central);root->addWidget(tabs);
        auto localPage=new QWidget(tabs);tabs->addTab(localPage,"Diagnosi locale");auto layout=new QVBoxLayout(localPage);
        auto header=new QLabel("<h2>NetLink Companion</h2>Analisi locale senza modifiche a servizi, firewall, rete o chiavi.",central);header->setWordWrap(true);layout->addWidget(header);
        auto bar=new QHBoxLayout;layout->addLayout(bar);
        m_scan=new QPushButton("Analizza PC",central);bar->addWidget(m_scan);
        m_role=new QComboBox(central);m_role->setObjectName("companionRole");m_role->addItem("Ruolo da scegliere","unspecified");m_role->addItem("PC docente / Master","teacher");m_role->addItem("PC studente / client","client");bar->addWidget(m_role);
        auto appearance=new QPushButton("Aspetto",central);bar->addWidget(appearance);
        auto elevate=new QPushButton("Apri come amministratore…",central);bar->addWidget(elevate);bar->addStretch();
        auto note=new QLabel("Per controllare i permessi del docente eseguire prima Companion normalmente, con lo stesso account usato per il Master. Un'analisi elevata verifica un contesto diverso.",central);note->setWordWrap(true);layout->addWidget(note);
        m_status=new QLabel("Pronto. L'analisi non invia dati ad altri PC.",central);m_status->setWordWrap(true);layout->addWidget(m_status);
        m_view=new QTextBrowser(central);m_view->setObjectName("companionReport");m_view->setOpenExternalLinks(false);m_view->setOpenLinks(false);layout->addWidget(m_view,1);
        auto share=new QHBoxLayout;layout->addLayout(share);
        m_identifiers=new QCheckBox("Includi identificativi, account e indirizzi nell'esportazione",central);share->addWidget(m_identifiers);
        m_export=new QPushButton("Esporta rapporto JSON…",central);share->addWidget(m_export);m_export->setEnabled(false);
        m_compare=new QPushButton("Confronta con altro PC…",central);share->addWidget(m_compare);m_compare->setEnabled(false);
        auto probeBox=new QGroupBox("Prova un collegamento scelto (nessuna scansione della rete)",central);layout->addWidget(probeBox);auto probeLayout=new QFormLayout(probeBox);
        m_target=new QComboBox(probeBox);m_target->setObjectName("companionTarget");m_target->setEditable(true);m_target->setInsertPolicy(QComboBox::NoInsert);probeLayout->addRow("Nome/IP destinatario",m_target);
        auto endpoint=new QHBoxLayout;m_port=new QSpinBox(probeBox);m_port->setRange(1,65535);m_port->setValue(11100);endpoint->addWidget(m_port);auto discovery=new QPushButton("Porta annunci docente: 3702",probeBox);endpoint->addWidget(discovery);m_probe=new QPushButton("Verifica TCP",probeBox);endpoint->addWidget(m_probe);probeLayout->addRow("Porta TCP",endpoint);
        m_port->setObjectName("companionPort");m_probe->setObjectName("companionProbe");
        auto warning=new QLabel("La prova verifica solo l'apertura TCP: non autentica il Master e non verifica il multicast UDP. Sul servizio Veyon/NetLink può comparire un avviso di autenticazione incompleta.",probeBox);warning->setWordWrap(true);probeLayout->addRow(warning);
        m_probeResult=new QLabel("Nessuna prova eseguita.",probeBox);m_probeResult->setWordWrap(true);m_probeResult->setTextFormat(Qt::PlainText);probeLayout->addRow(m_probeResult);
        m_probeResult->setObjectName("companionProbeResult");
        m_protocol=new QCheckBox("Verifica anche la risposta iniziale del protocollo RFB / NetLink",probeBox);m_protocol->setChecked(true);m_protocol->setObjectName("companionProtocol");probeLayout->addRow(m_protocol);
        setResult(m_probeResult,"idle","Nessuna prova eseguita. TCP e autenticazione sono verifiche distinte.");
        auto linkPage=new QWidget(tabs);tabs->addTab(linkPage,"Collegamento protetto");auto linkLayout=new QVBoxLayout(linkPage);
        auto linkNote=new QLabel("Canale TLS temporaneo indipendente dal servizio NetLink. Abbinamento esplicito, un solo rapporto, codice valido al massimo 5 minuti. Nessun comando remoto o modifica automatica al firewall. Il codice è riservato: condividerlo solo con l'altro operatore autorizzato.",linkPage);linkNote->setWordWrap(true);linkLayout->addWidget(linkNote);
        auto receiveBox=new QGroupBox("1. Sul PC che riceve il rapporto",linkPage);auto receiveForm=new QFormLayout(receiveBox);linkLayout->addWidget(receiveBox);
        m_bindAddress=new QComboBox(receiveBox);m_bindAddress->setObjectName("companionBindAddress");receiveForm->addRow("Interfaccia locale",m_bindAddress);
        m_linkPort=new QSpinBox(receiveBox);m_linkPort->setRange(0,65535);m_linkPort->setValue(11110);m_linkPort->setSpecialValueText("Automatica");receiveForm->addRow("Porta separata (0 = automatica)",m_linkPort);
        auto receiveButtons=new QHBoxLayout;auto receive=new QPushButton("Ricevi un rapporto…",receiveBox);auto stop=new QPushButton("Interrompi",receiveBox);auto copy=new QPushButton("Copia codice",receiveBox);receiveButtons->addWidget(receive);receiveButtons->addWidget(stop);receiveButtons->addWidget(copy);receiveForm->addRow(receiveButtons);
        m_pairCode=new QPlainTextEdit(receiveBox);m_pairCode->setReadOnly(true);m_pairCode->setMaximumHeight(78);m_pairCode->setObjectName("companionPairCode");receiveForm->addRow("Codice di abbinamento",m_pairCode);
        auto firewallNote=new QLabel("Se Windows blocca la porta temporanea, serve un'autorizzazione sul PC ricevente. Le regole del servizio NetLink non autorizzano automaticamente Companion. Limitare l'eventuale regola alla rete fidata e al PC interlocutore; non disattivare il firewall.",receiveBox);firewallNote->setWordWrap(true);receiveForm->addRow(firewallNote);
        auto sendBox=new QGroupBox("2. Sull'altro PC",linkPage);auto sendForm=new QFormLayout(sendBox);linkLayout->addWidget(sendBox);
        m_sendCode=new QLineEdit(sendBox);m_sendCode->setMaxLength(1024);m_sendCode->setObjectName("companionSendCode");sendForm->addRow("Incolla il codice ricevuto",m_sendCode);
        auto send=new QPushButton("Invia il rapporto locale…",sendBox);sendForm->addRow(send);
        m_linkStatus=new QLabel(linkPage);m_linkStatus->setWordWrap(true);m_linkStatus->setTextFormat(Qt::PlainText);linkLayout->addWidget(m_linkStatus);setResult(m_linkStatus,"idle","Canale chiuso. Nessuna porta diagnostica aperta.");
        m_peerView=new QTextBrowser(linkPage);m_peerView->setOpenLinks(false);m_peerView->setOpenExternalLinks(false);linkLayout->addWidget(m_peerView,1);
        m_link=new Companion::DiagnosticLink(this);
        connect(stop,&QPushButton::clicked,this,[this]{m_link->stop();setResult(m_linkStatus,"idle","Canale interrotto. Per riprovare generare un nuovo codice.");});
        connect(copy,&QPushButton::clicked,this,[this]{const auto code=m_link->pairingCode();if(!code.isEmpty()) QApplication::clipboard()->setText(code);});
        connect(m_link,&Companion::DiagnosticLink::stopped,this,[this]{m_pairCode->clear();});
        connect(receive,&QPushButton::clicked,this,[this]{
            if(m_report.isEmpty() || m_role->currentData()=="unspecified") {QMessageBox::information(this,"Ricezione","Completare prima l'analisi locale e scegliere il ruolo nella prima scheda.");return;}
            const QHostAddress address(m_bindAddress->currentData().toString());
            if(address.isNull()) {QMessageBox::information(this,"Ricezione","Scegliere un indirizzo attivo di questo PC.");return;}
            if(QMessageBox::question(this,"Apertura temporanea",QString("Aprire su %1 la sola ricezione TLS di un rapporto diagnostico? Il codice scade entro 5 minuti. Nessuna regola firewall sarà modificata.").arg(address.toString()))!=QMessageBox::Yes) return;
            m_peerView->clear();
            if(m_link->receiveOnce(address,quint16(m_linkPort->value()))) {m_pairCode->setPlainText(m_link->pairingCode());Companion::DiagnosticLink::Endpoint endpoint;Companion::DiagnosticLink::decodeCode(m_link->pairingCode(),endpoint);setResult(m_linkStatus,"waiting",QString("Ricezione TLS su %1:%2 per un solo rapporto. Trasferire il codice all'altro PC. Il codice scade entro 5 minuti.").arg(endpoint.address.toString()).arg(endpoint.port));}
        });
        connect(send,&QPushButton::clicked,this,[this]{
            Companion::DiagnosticLink::Endpoint endpoint;
            if(m_report.isEmpty() || !Companion::DiagnosticLink::decodeCode(m_sendCode->text().trimmed(),endpoint)) {QMessageBox::information(this,"Invio","Completare l'analisi locale e incollare un codice valido.");return;}
            if(QMessageBox::question(this,"Condivisione dati diagnostici",QString("Inviare a %1:%2 il rapporto completo di questo PC? Include account, indirizzi, SID, metadati delle chiavi e configurazione, ma NON password o contenuti delle chiavi private. Verificare che il codice provenga dal PC autorizzato.").arg(endpoint.address.toString()).arg(endpoint.port))!=QMessageBox::Yes) return;
            setResult(m_linkStatus,"waiting",QString("Connessione TLS verso %1:%2 e verifica dell'abbinamento in corso…").arg(endpoint.address.toString()).arg(endpoint.port));m_peerView->clear();m_link->sendReport(m_sendCode->text().trimmed(),m_report);
        });
        connect(m_link,&Companion::DiagnosticLink::reportSent,this,[this]{m_sendCode->clear();setResult(m_linkStatus,"success","Rapporto consegnato e confermato dall'altro Companion. Canale temporaneo terminato. L'autenticazione Veyon/NetLink NON è verificata da questo scambio.");});
        connect(m_link,&Companion::DiagnosticLink::failed,this,[this](const QString& message){if(m_link->pairingCode().isEmpty()) m_pairCode->clear();setResult(m_linkStatus,"failure",message);});
        connect(m_link,&Companion::DiagnosticLink::reportReceived,this,[this](const QJsonObject& other){
            m_pairCode->clear();setResult(m_linkStatus,"success","Rapporto ricevuto tramite abbinamento TLS. Ricezione chiusa. Il rapporto contiene dati dichiarati dall'altro Companion, non una certificazione dello stato del PC.");
            const auto results=m_role->currentData()=="teacher"?Companion::compare(m_report,other):Companion::compare(other,m_report);
            QString content="<h2>Confronto automatico dei due PC</h2><ul>";for(const auto& item:results) content+="<li>"+item.toHtmlEscaped()+"</li>";
            m_peerView->setHtml(content+"</ul>"+Companion::html(other,m_role->currentData()=="teacher"?"client":"teacher"));
        });
        m_collector=new QProcess(this);m_collector->setProcessChannelMode(QProcess::SeparateChannels);
        m_deadline=new QTimer(this);m_deadline->setSingleShot(true);
        connect(m_scan,&QPushButton::clicked,this,[this]{scan();});
        connect(m_role,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this]{render();});
        connect(appearance,&QPushButton::clicked,this,[this]{NetLinkAppearance::install()->showSettings(this);});
        connect(elevate,&QPushButton::clicked,this,[this]{
#ifdef Q_OS_WIN
            const auto path=QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
            const auto result=reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr,L"runas",reinterpret_cast<LPCWSTR>(path.utf16()),nullptr,nullptr,SW_SHOWNORMAL));
            if(result<=32) QMessageBox::information(this,"NetLink Companion","Avvio amministrativo annullato o non riuscito. La finestra attuale resta disponibile.");
#else
            QMessageBox::information(this,"NetLink Companion","L'elevazione integrata è disponibile soltanto su Windows.");
#endif
        });
        connect(m_collector,&QProcess::readyReadStandardOutput,this,[this]{
            m_output+=m_collector->readAllStandardOutput();
            if(m_output.size()>Companion::MaximumReportBytes) {m_failure="Analisi interrotta: rapporto oltre il limite di 2 MiB.";m_collector->kill();}
        });
        connect(m_collector,&QProcess::readyReadStandardError,this,[this]{m_collector->readAllStandardError();});
        connect(m_deadline,&QTimer::timeout,this,[this]{m_failure="Analisi interrotta dopo 30 secondi. Nessuna impostazione è stata modificata.";m_collector->kill();});
        connect(m_collector,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){if(error==QProcess::FailedToStart){m_deadline->stop();m_scan->setEnabled(true);m_status->setText("Impossibile avviare il raccoglitore locale. Verificare l'installazione di Companion.");}});
        connect(m_collector,QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),this,[this](int code,QProcess::ExitStatus status){
            m_deadline->stop();m_scan->setEnabled(true);m_output+=m_collector->readAllStandardOutput();QString message;
            if(!m_failure.isEmpty() || status!=QProcess::NormalExit || code!=0 || !Companion::parseReport(m_output,m_report,message)) {m_status->setText(m_failure.isEmpty()?"Analisi non completata. "+message:m_failure);m_output.clear();return;}
            m_output.clear();m_export->setEnabled(true);m_compare->setEnabled(true);m_status->setText("Analisi completata sul contesto corrente. Informazioni non disponibili e limiti sono indicati nel rapporto.");
            const auto settings=m_report["settings"].toObject();m_port->setValue(settings["Network/VeyonServerPort"].toInt(11100));m_target->clear();
            const auto master=settings["School/DiscoveryMaster"].toString();if(!master.isEmpty()) m_target->addItem(master);
            for(const auto& value:m_report["configuredComputers"].toArray()) {const auto address=value.toObject()["address"].toString();if(Companion::validTarget(address) && m_target->findText(address)<0) m_target->addItem(address);}
            m_bindAddress->clear();
            for(const auto& value:m_report["interfaces"].toArray()) {const auto iface=value.toObject();if(!iface["up"].toBool() || iface["loopback"].toBool()) continue;
                for(const auto& ip:iface["addresses"].toArray()) {const auto address=ip.toObject()["address"].toString();const QHostAddress parsed(address);if(parsed.protocol()!=QAbstractSocket::IPv4Protocol || parsed.isLinkLocal()) continue;
                    const auto label=iface["name"].toString()+" — "+address;if(!iface["gateways"].toArray().isEmpty()) m_bindAddress->insertItem(0,label,address);else m_bindAddress->addItem(label,address);}}
            render();
        });
        connect(m_export,&QPushButton::clicked,this,[this]{exportReport();});
        connect(m_compare,&QPushButton::clicked,this,[this]{compareReport();});
        connect(discovery,&QPushButton::clicked,this,[this]{m_port->setValue(3702);});
        m_socket=new QTcpSocket(this);m_probeDeadline=new QTimer(this);m_probeDeadline->setSingleShot(true);
        connect(m_probe,&QPushButton::clicked,this,[this]{
            const auto target=m_target->currentText().trimmed();if(!Companion::validTarget(target)){setResult(m_probeResult,"warning","Inserire un solo nome PC o indirizzo IP, senza URL, CIDR o numero di porta.");return;}
            m_wantProtocol=m_protocol->isChecked() && m_port->value()!=3702;m_probeConnected=false;m_probeBanner.clear();m_probe->setEnabled(false);setResult(m_probeResult,"waiting",QString("Prova verso %1:%2 in corso (massimo 8 secondi)…").arg(target).arg(m_port->value()));m_probeDeadline->start(8000);m_socket->connectToHost(target,quint16(m_port->value()));
        });
        connect(m_socket,&QTcpSocket::connected,this,[this]{m_probeConnected=true;m_probeEndpoint=QString("%1 → %2:%3").arg(m_socket->localAddress().toString(),m_socket->peerAddress().toString()).arg(m_socket->peerPort());
            setResult(m_probeResult,"success","TCP raggiungibile. "+m_probeEndpoint+". Autenticazione NON verificata.");
            if(!m_wantProtocol) finishProbe();else setResult(m_probeResult,"waiting","TCP raggiungibile. "+m_probeEndpoint+". Attendo la risposta iniziale del protocollo; autenticazione NON verificata.");});
        connect(m_socket,&QTcpSocket::readyRead,this,[this]{if(m_probe->isEnabled() || !m_probeConnected) return;
            m_probeBanner+=m_socket->read(12-m_probeBanner.size());if(m_probeBanner.size()<12) return;
            const bool valid=QRegularExpression("^RFB [0-9]{3}\\.[0-9]{3}\\n$").match(QString::fromLatin1(m_probeBanner)).hasMatch();
            setResult(m_probeResult,valid?"success":"warning",QString("TCP raggiungibile. %1. %2 Autenticazione NON verificata.").arg(m_probeEndpoint,valid?"Il servizio risponde con un banner RFB; non è ancora una prova di autenticazione NetLink.":"La risposta non è un banner RFB valido: verificare programma e porta."));finishProbe();});
        connect(m_socket,QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),this,[this](QAbstractSocket::SocketError){if(m_probe->isEnabled()) return;
            setResult(m_probeResult,m_probeConnected?"warning":"failure",m_probeConnected?"TCP raggiungibile ma risposta del protocollo interrotta. "+m_probeEndpoint+". Autenticazione NON verificata.":"TCP non raggiungibile: "+m_socket->errorString()+". Verificare servizio in ascolto, porta, regole firewall e percorso di rete; la causa non è ancora accertata.");finishProbe();});
        connect(m_probeDeadline,&QTimer::timeout,this,[this]{setResult(m_probeResult,m_probeConnected?"warning":"failure",m_probeConnected?"TCP raggiungibile ma nessuna risposta RFB completa entro il tempo previsto. "+m_probeEndpoint+". Controllare il servizio, non rigenerare le chiavi sulla base di questa prova.":"Tempo scaduto: TCP non raggiungibile. Se il client risponde in locale, verificare ascolto sulle interfacce di rete ed eccezione firewall sul destinatario. Non disattivare il firewall.");finishProbe();});
        if(!initialReport.isEmpty()) {m_report=initialReport;m_export->setEnabled(true);m_compare->setEnabled(true);render();}
        if(autoScan) QTimer::singleShot(0,this,[this]{scan();});
    }
protected:
    void closeEvent(QCloseEvent* event) override {
        if(m_collector->state()!=QProcess::NotRunning) {m_collector->kill();if(!m_collector->waitForFinished(1500)){event->ignore();m_status->setText("Attendere la chiusura del raccoglitore locale, poi riprovare.");return;}}
        finishProbe();m_link->stop();event->accept();
    }
private:
    static void setResult(QLabel* label,const QString& state,const QString& text) {
        auto palette=QApplication::palette();const bool dark=palette.color(QPalette::Window).lightness()<128;
        QColor background=palette.color(QPalette::Window),foreground=palette.color(QPalette::WindowText);
        bool highContrast=false;
#ifdef Q_OS_WIN
        HIGHCONTRASTW contrast{};contrast.cbSize=sizeof(contrast);highContrast=SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(contrast),&contrast,0) && (contrast.dwFlags&HCF_HIGHCONTRASTON);
#endif
        if(!highContrast && state=="success") {background=QColor(dark?"#174631":"#c7f0d8");foreground=QColor(dark?"#e0f8ea":"#123c28");}
        else if(!highContrast && state=="failure") {background=QColor(dark?"#602323":"#ffd7d7");foreground=QColor(dark?"#fff0f0":"#641818");}
        else if(!highContrast && (state=="warning" || state=="waiting")) {background=QColor(dark?"#51421a":"#fff1c7");foreground=QColor(dark?"#fff3c4":"#573d00");}
        palette.setColor(QPalette::Window,background);palette.setColor(QPalette::WindowText,foreground);label->setPalette(palette);label->setAutoFillBackground(true);label->setMargin(10);
        auto font=label->font();font.setBold(true);label->setFont(font);label->setProperty("diagnosticState",state);label->setText(text);label->setAccessibleName("Esito diagnostico: "+state);
    }
    void scan(){if(m_collector->state()!=QProcess::NotRunning) return;m_report={};m_output.clear();m_failure.clear();m_export->setEnabled(false);m_compare->setEnabled(false);m_scan->setEnabled(false);m_view->clear();m_status->setText("Analisi locale in corso (massimo 30 secondi)…");m_collector->start(QCoreApplication::applicationFilePath(),{"--collect"});m_deadline->start(30000);}
    void render(){if(!m_report.isEmpty()) m_view->setHtml(Companion::html(m_report,m_role->currentData().toString()));}
    void finishProbe(){m_probeDeadline->stop();m_probe->setEnabled(true);m_socket->abort();}
    void exportReport(){
        if(m_identifiers->isChecked() && QMessageBox::question(this,"Dati del rapporto","Il rapporto completo contiene nomi PC/account, SID, indirizzi IP/MAC, configurazione e permessi. Non include password o contenuti di chiavi private. Conservarlo e condividerlo soltanto con persone autorizzate. Continuare?")!=QMessageBox::Yes) return;
        const auto path=QFileDialog::getSaveFileName(this,"Esporta rapporto Companion","NetLink-Companion.json","Rapporto JSON (*.json)");if(path.isEmpty()) return;
        QSaveFile file(path);const auto bytes=QJsonDocument(Companion::publicReport(m_report,m_identifiers->isChecked())).toJson(QJsonDocument::Indented);
        if(!file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size() || !file.commit()) QMessageBox::warning(this,"NetLink Companion","Salvataggio del rapporto non riuscito.");
    }
    void compareReport(){
        if(m_role->currentData()=="unspecified") {QMessageBox::information(this,"Confronto","Selezionare prima il ruolo di questo PC: docente oppure client.");return;}
        const auto path=QFileDialog::getOpenFileName(this,"Apri rapporto dell'altro PC",{},"Rapporto JSON (*.json)");if(path.isEmpty()) return;
        QFile file(path);if(!file.open(QIODevice::ReadOnly)){QMessageBox::warning(this,"Confronto","Rapporto non leggibile.");return;}QJsonObject other;QString message;
        if(!Companion::parseReport(file.read(Companion::MaximumReportBytes+1),other,message)){QMessageBox::warning(this,"Confronto",message);return;}
        const auto results=m_role->currentData()=="teacher"?Companion::compare(m_report,other):Companion::compare(other,m_report);
        auto dialog=new QDialog(this);dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setWindowTitle("Confronto docente / client — rapporti non firmati");dialog->resize(800,500);auto layout=new QVBoxLayout(dialog);auto view=new QTextBrowser(dialog);view->setOpenLinks(false);QString content="<h2>Risultati del confronto</h2><ul>";for(const auto& result:results) content+="<li>"+result.toHtmlEscaped()+"</li>";view->setHtml(content+"</ul>");layout->addWidget(view);dialog->show();
    }
    QJsonObject m_report;QByteArray m_output;QString m_failure;
    QPushButton *m_scan,*m_export,*m_compare,*m_probe;QComboBox *m_role,*m_target;QSpinBox* m_port;QCheckBox* m_identifiers;QLabel *m_status,*m_probeResult;QTextBrowser* m_view;QProcess* m_collector;QTcpSocket* m_socket;QTimer *m_deadline,*m_probeDeadline;
    QCheckBox* m_protocol;bool m_probeConnected=false,m_wantProtocol=false;QByteArray m_probeBanner;QString m_probeEndpoint;
    Companion::DiagnosticLink* m_link;QComboBox* m_bindAddress;QSpinBox* m_linkPort;QPlainTextEdit* m_pairCode;QLineEdit* m_sendCode;QLabel* m_linkStatus;QTextBrowser* m_peerView;
};

#ifndef NETLINK_COMPANION_WIDGET_TEST
int main(int argc,char** argv) {
    // The collector is a bounded child process; no VeyonCore initialization,
    // plugin loading, service control, firewall changes or authentication writes.
    const bool collecting=argc==2 && QByteArray(argv[1])=="--collect";
    if(argc==2 && QByteArray(argv[1])=="--version") {const QByteArray version(VEYON_VERSION);std::fwrite(version.constData(),1,size_t(version.size()),stdout);return 0;}
    if(collecting) {
        QCoreApplication app(argc,argv);app.setApplicationVersion(VEYON_VERSION);
        const auto bytes=QJsonDocument(Companion::collect()).toJson(QJsonDocument::Compact);
        if(bytes.size()>Companion::MaximumReportBytes) return 2;
        return std::fwrite(bytes.constData(),1,size_t(bytes.size()),stdout)==size_t(bytes.size())?0:3;
    }
    QApplication app(argc,argv);app.setOrganizationName("Veyon");app.setApplicationName("NetLinkCompanion");app.setApplicationDisplayName("NetLink Companion");app.setApplicationVersion(VEYON_VERSION);app.setWindowIcon(QIcon(":/companion/icon.png"));
    NetLinkAppearance::install();CompanionWindow window;window.show();return app.exec();
}
#endif
