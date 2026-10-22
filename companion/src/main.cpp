// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025-26 prof. Ing. Raffaele Mele
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include "Diagnostics.h"
#include "DiagnosticLink.h"
#include "NetLinkAppearance.h"
#include "NetLinkUi.h"
#include "GuiLatencyMonitor.h"
#include <veyonconfig.h>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QDateTime>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QFontDatabase>
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
        setWindowTitle(QStringLiteral("NetLink Companion"));
        setWindowIcon(QIcon(":/companion/icon.png"));resize(1050,800);
        auto central=new QWidget(this);setCentralWidget(central);auto root=new QVBoxLayout(central);
        auto tabs=new QTabWidget(central);root->addWidget(tabs);
        NetLinkUi::addInformationMenu(this);
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
        auto probeBox=new QGroupBox("Prove di comunicazione — scegliere cosa verificare",central);layout->addWidget(probeBox);auto probeLayout=new QFormLayout(probeBox);
        m_testType=new QComboBox(probeBox);m_testType->setObjectName("companionTestType");
        m_testType->addItem("Controllo remoto — docente → client","control");m_testType->addItem("Discovery diretto — client → docente","discovery");m_testType->addItem("TCP personalizzato — diagnosi avanzata","custom");probeLayout->addRow("Tipo di prova",m_testType);
        m_probeGuide=new QLabel(probeBox);m_probeGuide->setWordWrap(true);m_probeGuide->setTextFormat(Qt::PlainText);m_probeGuide->setObjectName("companionProbeGuide");probeLayout->addRow(m_probeGuide);
        m_target=new QComboBox(probeBox);m_target->setObjectName("companionTarget");m_target->setEditable(true);m_target->setInsertPolicy(QComboBox::NoInsert);probeLayout->addRow("Nome/IP destinatario",m_target);
        auto endpoint=new QHBoxLayout;m_port=new QSpinBox(probeBox);m_port->setRange(1,65535);m_port->setValue(11100);endpoint->addWidget(m_port);m_probe=new QPushButton("Esegui prova",probeBox);endpoint->addWidget(m_probe);probeLayout->addRow("Porta TCP del destinatario",endpoint);
        m_port->setObjectName("companionPort");m_probe->setObjectName("companionProbe");
        auto warning=new QLabel("La prova verifica solo l'apertura TCP: non autentica il Master e non verifica il multicast UDP. Sul servizio Veyon/NetLink può comparire un avviso di autenticazione incompleta.",probeBox);warning->setWordWrap(true);probeLayout->addRow(warning);
        m_probeResult=new QLabel("Nessuna prova eseguita.",probeBox);m_probeResult->setWordWrap(true);m_probeResult->setTextFormat(Qt::PlainText);probeLayout->addRow(m_probeResult);
        m_probeResult->setObjectName("companionProbeResult");
        m_protocol=new QCheckBox("Verifica anche la risposta iniziale del protocollo RFB / NetLink",probeBox);m_protocol->setChecked(true);m_protocol->setObjectName("companionProtocol");probeLayout->addRow(m_protocol);
        setResult(m_probeResult,"idle","Nessuna prova eseguita. TCP e autenticazione sono verifiche distinte.");
        auto linkPage=new QWidget(tabs);tabs->addTab(linkPage,"Collegamento protetto");auto linkLayout=new QVBoxLayout(linkPage);
        auto linkNote=new QLabel("Procedura consigliata fra docente e client:\n1. Sul DOCENTE: Analizza PC, scegli ruolo docente e «Ricevo il rapporto». Genera l'invito.\n2. Trasferisci l'invito al CLIENT, con copia/incolla oppure salvandolo su file.\n3. Sul CLIENT: Analizza PC, scegli ruolo client e «Invio il mio rapporto». Incolla o apri l'invito, poi conferma l'invio.\n4. Torna sul DOCENTE: qui appariranno il rapporto del client e il confronto automatico.\nIl canale è separato da controllo remoto e discovery: non ripara o configura i PC. Non condividere l'invito pubblicamente.",linkPage);linkNote->setWordWrap(true);linkNote->setTextFormat(Qt::PlainText);linkNote->setObjectName("companionLinkGuide");linkLayout->addWidget(linkNote);
        auto linkOptions=new QFormLayout;linkLayout->addLayout(linkOptions);
        m_linkRole=new QComboBox(linkPage);for(int i=0;i<m_role->count();++i) m_linkRole->addItem(m_role->itemText(i),m_role->itemData(i));linkOptions->addRow("Ruolo di QUESTO PC",m_linkRole);
        m_linkAction=new QComboBox(linkPage);m_linkAction->setObjectName("companionLinkAction");m_linkAction->addItem("Ricevo il rapporto dell'altro PC (consigliato sul docente)");m_linkAction->addItem("Invio il mio rapporto (consigliato sul client)");linkOptions->addRow("Su QUESTO PC voglio…",m_linkAction);
        auto receiveBox=new QGroupBox("Sul ricevente: genera e trasferisci l'invito, poi attendi",linkPage);m_receiveBox=receiveBox;auto receiveForm=new QFormLayout(receiveBox);linkLayout->addWidget(receiveBox);
        m_bindAddress=new QComboBox(receiveBox);m_bindAddress->setObjectName("companionBindAddress");receiveForm->addRow("Interfaccia locale",m_bindAddress);
        m_linkPort=new QSpinBox(receiveBox);m_linkPort->setRange(0,65535);m_linkPort->setValue(11110);m_linkPort->setSpecialValueText("Automatica");receiveForm->addRow("Porta separata (0 = automatica)",m_linkPort);
        auto receiveButtons=new QHBoxLayout;m_receive=new QPushButton("1. Genera invito e attendi…",receiveBox);m_copyCode=new QPushButton("2. Copia invito",receiveBox);m_saveInvite=new QPushButton("Salva invito su file…",receiveBox);receiveButtons->addWidget(m_receive);receiveButtons->addWidget(m_copyCode);receiveButtons->addWidget(m_saveInvite);receiveForm->addRow(receiveButtons);
        m_pairCode=new QPlainTextEdit(receiveBox);m_pairCode->setReadOnly(true);m_pairCode->setMaximumHeight(58);m_pairCode->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));m_pairCode->setObjectName("companionPairCode");receiveForm->addRow("Invito monouso (non è una chiave Veyon)",m_pairCode);
        m_linkRemaining=new QLabel("Nessun invito attivo.",receiveBox);receiveForm->addRow(m_linkRemaining);
        auto firewallNote=new QLabel("Se Windows blocca la porta temporanea, serve un'autorizzazione sul PC ricevente. Le regole del servizio NetLink non autorizzano automaticamente Companion. Limitare l'eventuale regola alla rete fidata e al PC interlocutore; non disattivare il firewall.",receiveBox);firewallNote->setWordWrap(true);receiveForm->addRow(firewallNote);
        auto sendBox=new QGroupBox("Sul mittente: usa l'invito del ricevente e invia i tuoi dati",linkPage);m_sendBox=sendBox;auto sendForm=new QFormLayout(sendBox);linkLayout->addWidget(sendBox);sendBox->hide();
        m_sendCode=new QLineEdit(sendBox);m_sendCode->setMaxLength(2048);m_sendCode->setClearButtonEnabled(true);m_sendCode->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));m_sendCode->setObjectName("companionSendCode");m_sendCode->setPlaceholderText("NL2.… — incolla l'invito generato sull'altro PC");sendForm->addRow("1. Incolla l'invito ricevuto",m_sendCode);
        auto openInvite=new QPushButton("Oppure apri il file invito…",sendBox);sendForm->addRow(openInvite);
        m_send=new QPushButton("2. Verifica destinatario e invia il mio rapporto…",sendBox);sendForm->addRow(m_send);
        m_stopLink=new QPushButton("Annulla collegamento / chiudi ricezione",linkPage);linkLayout->addWidget(m_stopLink);
        m_linkStatus=new QLabel(linkPage);m_linkStatus->setObjectName("companionLinkStatus");m_linkStatus->setWordWrap(true);m_linkStatus->setTextFormat(Qt::PlainText);linkLayout->addWidget(m_linkStatus);setResult(m_linkStatus,"idle","Canale chiuso. Nessuna porta diagnostica aperta.");
        m_peerView=new QTextBrowser(linkPage);m_peerView->setOpenLinks(false);m_peerView->setOpenExternalLinks(false);linkLayout->addWidget(m_peerView,1);
        m_link=new Companion::DiagnosticLink(this);
        m_linkCountdown=new QTimer(this);m_linkCountdown->setInterval(1000);
        connect(m_linkCountdown,&QTimer::timeout,this,[this]{if(m_link->pairingCode().isEmpty()) {m_pairCode->clear();m_copyCode->setEnabled(false);m_saveInvite->setEnabled(false);m_linkRemaining->setText("Invito utilizzato o scaduto.");m_linkCountdown->stop();return;}const int seconds=qMax(0,300-int(m_pairClock.elapsed()/1000));m_linkRemaining->setText(QString("Invito valido ancora %1:%2 — %3 caratteri. Ora trasferiscilo all'altro PC.").arg(seconds/60).arg(seconds%60,2,10,QChar('0')).arg(m_link->pairingCode().size()));});
        connect(m_linkAction,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](int index){m_receiveBox->setVisible(index==0);m_sendBox->setVisible(index==1);});
        connect(m_linkRole,QOverload<int>::of(&QComboBox::currentIndexChanged),m_role,&QComboBox::setCurrentIndex);
        connect(m_stopLink,&QPushButton::clicked,this,[this]{m_link->stop();setResult(m_linkStatus,"idle","Canale interrotto. Nessuna porta diagnostica aperta. Per riprovare generare un nuovo invito.");});
        connect(m_copyCode,&QPushButton::clicked,this,[this]{const auto code=m_link->pairingCode();if(!code.isEmpty()) QApplication::clipboard()->setText(code);});
        connect(m_saveInvite,&QPushButton::clicked,this,[this]{const auto code=m_link->pairingCode();if(code.isEmpty()) return;const auto path=QFileDialog::getSaveFileName(this,"Salva invito riservato — scade entro 5 minuti","NetLink.netlink-invite","Invito NetLink (*.netlink-invite)");if(path.isEmpty()) return;QSaveFile file(path);const auto bytes=code.toUtf8();if(!file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size() || !file.commit()) QMessageBox::warning(this,"Invito","Salvataggio non riuscito.");});
        connect(openInvite,&QPushButton::clicked,this,[this]{const auto path=QFileDialog::getOpenFileName(this,"Apri invito ricevuto dall'altro PC",{},"Invito NetLink (*.netlink-invite)");if(path.isEmpty()) return;QFile file(path);Companion::DiagnosticLink::Endpoint endpoint;if(!file.open(QIODevice::ReadOnly) || file.size()>2048) {QMessageBox::warning(this,"Invito","File invito non leggibile o troppo grande.");return;}const auto code=QString::fromUtf8(file.read(2049));if(!Companion::DiagnosticLink::decodeCode(code,endpoint)) {QMessageBox::warning(this,"Invito","Invito non valido o danneggiato.");return;}m_sendCode->setText(code);});
        connect(m_link,&Companion::DiagnosticLink::stopped,this,[this]{m_pairCode->clear();m_linkCountdown->stop();m_linkRemaining->setText("Nessun invito attivo.");setLinkBusy(false);});
        connect(m_receive,&QPushButton::clicked,this,[this]{
            if(m_report.isEmpty() || m_role->currentData()=="unspecified") {QMessageBox::information(this,"Ricezione","Completare prima l'analisi locale e scegliere il ruolo nella prima scheda.");return;}
            const QHostAddress address(m_bindAddress->currentData().toString());
            if(address.isNull()) {QMessageBox::information(this,"Ricezione","Scegliere un indirizzo attivo di questo PC.");return;}
            if(QMessageBox::question(this,"Apertura temporanea",QString("Aprire su %1 la sola ricezione TLS di un rapporto diagnostico? Il codice scade entro 5 minuti. Nessuna regola firewall sarà modificata.").arg(address.toString()))!=QMessageBox::Yes) return;
            m_peerView->clear();
            if(m_link->receiveOnce(address,quint16(m_linkPort->value()))) {setLinkBusy(true);m_pairClock.start();m_linkCountdown->start();m_linkRemaining->setText(QString("Invito valido per 5 minuti — %1 caratteri.").arg(m_link->pairingCode().size()));m_pairCode->setPlainText(m_link->pairingCode());Companion::DiagnosticLink::Endpoint endpoint;Companion::DiagnosticLink::decodeCode(m_link->pairingCode(),endpoint);setResult(m_linkStatus,"waiting",QString("IN ATTESA su %1:%2. Ora vai sull'altro PC, scegli «Invio il mio rapporto», incolla/apri questo invito e conferma l'invio. Il confronto comparirà QUI. La Console docente può restare chiusa.").arg(endpoint.address.toString()).arg(endpoint.port));}
        });
        connect(m_send,&QPushButton::clicked,this,[this]{
            Companion::DiagnosticLink::Endpoint endpoint;
            if(m_report.isEmpty() || m_role->currentData()=="unspecified" || !Companion::DiagnosticLink::decodeCode(m_sendCode->text().trimmed(),endpoint)) {QMessageBox::information(this,"Invio","Completare l'analisi locale, scegliere il ruolo di questo PC e incollare un invito valido generato dall'altro PC.");return;}
            if(QMessageBox::question(this,"Condivisione dati diagnostici",QString("Inviare a %1:%2 il rapporto completo di questo PC? Include account, indirizzi, SID, metadati delle chiavi e configurazione, ma NON password o contenuti delle chiavi private. Verificare che il codice provenga dal PC autorizzato.").arg(endpoint.address.toString()).arg(endpoint.port))!=QMessageBox::Yes) return;
            m_peerView->clear();const bool started=m_link->sendReport(m_sendCode->text().trimmed(),m_report);if(started) {setLinkBusy(true);setResult(m_linkStatus,"waiting",QString("INVIO verso %1:%2: verifica TLS e consegna del rapporto in corso. Attendi la conferma, poi torna sul PC ricevente per leggere il confronto.").arg(endpoint.address.toString()).arg(endpoint.port));}
        });
        connect(m_link,&Companion::DiagnosticLink::reportSent,this,[this]{setLinkBusy(false);m_sendCode->clear();setResult(m_linkStatus,"success","CONSEGNATO: l'altro Companion ha confermato il rapporto. Torna sul PC ricevente per leggere il confronto automatico. Canale terminato; autenticazione NetLink NON verificata.");});
        connect(m_link,&Companion::DiagnosticLink::failed,this,[this](const QString& message){if(m_link->pairingCode().isEmpty()) {m_pairCode->clear();setLinkBusy(false);}setResult(m_linkStatus,"failure",message+" Se il tempo è scaduto, genera un NUOVO invito sul ricevente; non riutilizzare quello vecchio.");});
        connect(m_link,&Companion::DiagnosticLink::reportReceived,this,[this](const QJsonObject& other){
            m_pairCode->clear();setLinkBusy(false);m_linkCountdown->stop();m_linkRemaining->setText("Invito utilizzato: non può essere riutilizzato.");setResult(m_linkStatus,"success","RICEVUTO: confronto disponibile qui sotto. Ricezione chiusa. I dati dichiarati dall'altro Companion non certificano lo stato del PC.");
            const auto results=m_role->currentData()=="teacher"?Companion::compare(m_report,other):Companion::compare(other,m_report);
            QString content="<h2>Confronto automatico dei due PC</h2><ul>";for(const auto& item:results) content+="<li>"+item.toHtmlEscaped()+"</li>";
            m_peerView->setHtml(content+"</ul>"+Companion::html(other,m_role->currentData()=="teacher"?"client":"teacher"));
        });
        m_collector=new QProcess(this);m_collector->setProcessChannelMode(QProcess::SeparateChannels);
        m_deadline=new QTimer(this);m_deadline->setSingleShot(true);
        connect(m_scan,&QPushButton::clicked,this,[this]{scan();});
        connect(m_role,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](int index){m_linkRole->setCurrentIndex(index);m_linkAction->setCurrentIndex(m_role->currentData()=="client"?1:0);m_testType->setCurrentIndex(m_role->currentData()=="client"?1:0);configureProbe();render();});
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
            m_report["connectionTests"]=m_probeHistory;populateInterfaces();configureProbe();render();
        });
        connect(m_export,&QPushButton::clicked,this,[this]{exportReport();});
        connect(m_compare,&QPushButton::clicked,this,[this]{compareReport();});
        connect(m_testType,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this]{configureProbe();});
        m_socket=new QTcpSocket(this);m_probeDeadline=new QTimer(this);m_probeDeadline->setSingleShot(true);
        connect(m_probe,&QPushButton::clicked,this,[this]{
            const auto target=m_target->currentText().trimmed();if(!Companion::validTarget(target)){setResult(m_probeResult,"warning","Inserire un solo nome PC o indirizzo IP, senza URL, CIDR o numero di porta.");return;}
            m_activeTest=m_testType->currentData().toString();m_requestedTarget=target;m_requestedPort=m_port->value();m_probeStarted=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
            m_wantProtocol=m_protocol->isChecked() && m_activeTest!="discovery";m_probeConnected=false;m_probeBanner.clear();m_probeEndpoint.clear();m_probe->setEnabled(false);setProbeControls(false);setResult(m_probeResult,"waiting",QString("%1 — verso %2:%3 in corso (massimo 8 secondi)…").arg(probeName(),target).arg(m_port->value()));m_probeDeadline->start(8000);m_socket->connectToHost(target,quint16(m_port->value()));
        });
        connect(m_socket,&QTcpSocket::connected,this,[this]{m_probeConnected=true;m_probeEndpoint=QString("%1 → %2:%3").arg(m_socket->localAddress().toString(),m_socket->peerAddress().toString()).arg(m_socket->peerPort());
            setResult(m_probeResult,"success",probeName()+" — TCP raggiungibile. "+m_probeEndpoint+(m_activeTest=="discovery"?". Porta degli annunci diretti aperta; annuncio applicativo e multicast UDP NON verificati.":". Autenticazione NON verificata."));
            if(!m_wantProtocol) finishProbe();else setResult(m_probeResult,"waiting","TCP raggiungibile. "+m_probeEndpoint+". Attendo la risposta iniziale del protocollo; autenticazione NON verificata.");});
        connect(m_socket,&QTcpSocket::readyRead,this,[this]{if(m_probe->isEnabled() || !m_probeConnected) return;
            m_probeBanner+=m_socket->read(12-m_probeBanner.size());if(m_probeBanner.size()<12) return;
            const bool valid=QRegularExpression("^RFB [0-9]{3}\\.[0-9]{3}\\n$").match(QString::fromLatin1(m_probeBanner)).hasMatch();
            setResult(m_probeResult,valid?"success":"warning",QString("%1 — TCP raggiungibile. %2. %3 Autenticazione NON verificata.").arg(probeName(),m_probeEndpoint,valid?"Il servizio risponde con un banner RFB; non è ancora una prova di autenticazione NetLink.":"La risposta non è un banner RFB valido: verificare programma e porta."));finishProbe();});
        connect(m_socket,QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),this,[this](QAbstractSocket::SocketError){if(m_probe->isEnabled()) return;
            setResult(m_probeResult,m_probeConnected?"warning":"failure",probeName()+" — "+(m_probeConnected?"TCP raggiungibile ma risposta del protocollo interrotta. "+m_probeEndpoint+". Autenticazione NON verificata.":"TCP non raggiungibile verso "+m_requestedTarget+":"+QString::number(m_requestedPort)+": "+m_socket->errorString()+". "+probeFailureHint()));finishProbe();});
        connect(m_probeDeadline,&QTimer::timeout,this,[this]{setResult(m_probeResult,m_probeConnected?"warning":"failure",probeName()+" — "+(m_probeConnected?"TCP raggiungibile ma nessuna risposta RFB completa entro il tempo previsto. "+m_probeEndpoint+". Non rigenerare le chiavi sulla base di questa prova.":"Tempo scaduto verso "+m_requestedTarget+":"+QString::number(m_requestedPort)+". "+probeFailureHint()));finishProbe();});
        if(!initialReport.isEmpty()) {m_report=initialReport;m_export->setEnabled(true);m_compare->setEnabled(true);populateInterfaces();render();}
        configureProbe();setLinkBusy(false);
        if(autoScan) QTimer::singleShot(0,this,[this]{scan();});
    }
protected:
    void closeEvent(QCloseEvent* event) override {
        if(m_collector->state()!=QProcess::NotRunning) {m_collector->kill();if(!m_collector->waitForFinished(1500)){event->ignore();m_status->setText("Attendere la chiusura del raccoglitore locale, poi riprovare.");return;}}
        finishProbe();m_link->stop();event->accept();
    }
private:
    friend class CompanionTests;
    void populateInterfaces() {
        m_bindAddress->clear();
        for(const auto& value:m_report["interfaces"].toArray()) {const auto iface=value.toObject();if(!iface["up"].toBool() || iface["loopback"].toBool()) continue;
            for(const auto& ip:iface["addresses"].toArray()) {const auto address=ip.toObject()["address"].toString();const QHostAddress parsed(address);if(parsed.protocol()!=QAbstractSocket::IPv4Protocol || parsed.isLinkLocal()) continue;
                const auto name=iface["name"].toString()+" — "+address;if(!iface["gateways"].toArray().isEmpty()) m_bindAddress->insertItem(0,name,address);else m_bindAddress->addItem(name,address);}}
    }
    void configureProbe() {
        const auto type=m_testType->currentData().toString(),role=m_role->currentData().toString();const auto settings=m_report["settings"].toObject();
        m_protocol->setEnabled(type!="discovery");m_port->setReadOnly(type=="discovery");if(type=="custom") {m_probeGuide->setText("Diagnosi avanzata: scegli esplicitamente destinatario e porta. Un collegamento in direzione opposta non verifica quello usato dalla Console docente.");return;}
        m_port->setValue(type=="discovery"?3702:settings["Network/VeyonServerPort"].toInt(11100));m_target->clear();
        if(type=="discovery") {
            const auto master=settings["School/DiscoveryMaster"].toString();if(Companion::validTarget(master)) m_target->addItem(master);
            m_probeGuide->setText((role=="teacher"?"ATTENZIONE: questa prova normalmente si esegue sul CLIENT, non sul docente.\n":"")+QString("Sul CLIENT inserisci il nome/IP del DOCENTE e lascia aperta la Console docente su quel PC. Verifica TCP 3702 degli annunci diretti, NON il multicast WS-Discovery UDP 3702. Funziona anche tra subnet se routing e firewall lo consentono."));
        } else {
            for(const auto& value:m_report["configuredComputers"].toArray()) {const auto address=value.toObject()["address"].toString();if(Companion::validTarget(address) && m_target->findText(address)<0) m_target->addItem(address);}
            m_probeGuide->setText((role=="client"?"ATTENZIONE: questa prova normalmente si esegue sul DOCENTE, non sul client.\n":"")+QString("Sul DOCENTE scegli il CLIENT da controllare: il suo servizio NetLink deve essere attivo. La prova inversa client → docente non è necessaria per controllare il client. Porta abituale 11100; puoi indicare una porta client personalizzata."));
        }
        setResult(m_probeResult,"idle","Nessuna prova eseguita per questa selezione. I risultati precedenti restano nel rapporto JSON completo.");
    }
    QString probeName() const {return m_activeTest=="discovery"?"Discovery diretto (client → docente)":m_activeTest=="control"?"Controllo remoto (docente → client)":"TCP personalizzato";}
    QString probeFailureHint() const {return m_activeTest=="discovery"?"Sul DOCENTE apri la Console e verifica le regole TCP degli annunci. Le regole del servizio 11100 non bastano. Nessuna conclusione sul multicast o sulle chiavi.":"Sul destinatario verifica servizio, porta, ascolto sulle interfacce e firewall. Una prova locale 127.0.0.1 distingue ascolto da accesso remoto. Non disattivare il firewall: la causa non è ancora accertata.";}
    void setProbeControls(bool enabled) {m_testType->setEnabled(enabled);m_target->setEnabled(enabled);m_port->setEnabled(enabled);m_protocol->setEnabled(enabled && m_testType->currentData()!="discovery");m_role->setEnabled(enabled && !m_linkBusy);m_linkRole->setEnabled(enabled && !m_linkBusy);m_scan->setEnabled(enabled && !m_linkBusy && m_collector->state()==QProcess::NotRunning);}
    void setLinkBusy(bool busy) {m_linkBusy=busy;m_receive->setEnabled(!busy);m_send->setEnabled(!busy);m_stopLink->setEnabled(busy);m_linkAction->setEnabled(!busy);m_linkRole->setEnabled(!busy && m_probe->isEnabled());m_bindAddress->setEnabled(!busy);m_linkPort->setEnabled(!busy);m_copyCode->setEnabled(busy && !m_link->pairingCode().isEmpty());m_saveInvite->setEnabled(m_copyCode->isEnabled());m_role->setEnabled(!busy && m_probe->isEnabled());m_scan->setEnabled(!busy && m_collector->state()==QProcess::NotRunning && m_probe->isEnabled());}
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
    void finishProbe(){const bool completed=!m_probe->isEnabled();m_probeDeadline->stop();m_probe->setEnabled(true);m_socket->abort();if(completed && !m_report.isEmpty()) {m_probeHistory.append(QJsonObject{{"startedUtc",m_probeStarted},{"finishedUtc",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},{"kind",m_activeTest},{"sourceRole",m_role->currentData().toString()},{"target",m_requestedTarget},{"port",m_requestedPort},{"actualEndpoint",m_probeEndpoint},{"tcpReachable",m_probeConnected},{"state",m_probeResult->property("diagnosticState").toString()},{"result",m_probeResult->text()},{"authenticationVerified",false},{"multicastVerified",false}});while(m_probeHistory.size()>20) m_probeHistory.removeFirst();m_report["connectionTests"]=m_probeHistory;render();}setProbeControls(true);}
    void exportReport(){
        if(m_identifiers->isChecked() && QMessageBox::question(this,"Dati del rapporto","Il rapporto completo contiene nomi PC/account, SID, indirizzi IP/MAC, configurazione e permessi. Non include password o contenuti di chiavi private. Conservarlo e condividerlo soltanto con persone autorizzate. Continuare?")!=QMessageBox::Yes) return;
        const auto path=QFileDialog::getSaveFileName(this,"Esporta rapporto Companion","NetLink-Companion.json","Rapporto JSON (*.json)");if(path.isEmpty()) return;
        QSaveFile file(path);const auto bytes=QJsonDocument(Companion::exportedReport(m_report,m_identifiers->isChecked())).toJson(QJsonDocument::Indented);
        if(!file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size() || !file.commit()) QMessageBox::warning(this,"NetLink Companion","Salvataggio del rapporto non riuscito.");
    }
    void compareReport(){
        if(m_role->currentData()=="unspecified") {QMessageBox::information(this,"Confronto","Selezionare prima il ruolo di questo PC: docente oppure client.");return;}
        const auto path=QFileDialog::getOpenFileName(this,"Apri rapporto dell'altro PC",{},"Rapporto JSON (*.json)");if(path.isEmpty()) return;
        QFile file(path);if(!file.open(QIODevice::ReadOnly)){QMessageBox::warning(this,"Confronto","Rapporto non leggibile.");return;}QJsonObject other;QString message;
        if(!Companion::parseReport(file.read(Companion::MaximumReportBytes+1),other,message)){QMessageBox::warning(this,"Confronto",message);return;}
        const auto results=m_role->currentData()=="teacher"?Companion::compare(m_report,other):Companion::compare(other,m_report);
        auto dialog=new QDialog(this);dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setWindowTitle(NetLinkUi::windowTitle("Confronto docente / client — rapporti non firmati"));dialog->resize(800,500);auto layout=new QVBoxLayout(dialog);auto view=new QTextBrowser(dialog);view->setOpenLinks(false);QString content="<h2>Risultati del confronto</h2><ul>";for(const auto& result:results) content+="<li>"+result.toHtmlEscaped()+"</li>";view->setHtml(content+"</ul>");layout->addWidget(view);dialog->show();
    }
    QJsonObject m_report;QByteArray m_output;QString m_failure;
    QPushButton *m_scan,*m_export,*m_compare,*m_probe;QComboBox *m_role,*m_target;QSpinBox* m_port;QCheckBox* m_identifiers;QLabel *m_status,*m_probeResult;QTextBrowser* m_view;QProcess* m_collector;QTcpSocket* m_socket;QTimer *m_deadline,*m_probeDeadline;
    QCheckBox* m_protocol;bool m_probeConnected=false,m_wantProtocol=false;QByteArray m_probeBanner;QString m_probeEndpoint;
    QComboBox* m_testType;QLabel* m_probeGuide;QString m_activeTest,m_requestedTarget,m_probeStarted;int m_requestedPort=0;QJsonArray m_probeHistory;
    Companion::DiagnosticLink* m_link;QComboBox* m_bindAddress;QSpinBox* m_linkPort;QPlainTextEdit* m_pairCode;QLineEdit* m_sendCode;QLabel* m_linkStatus;QTextBrowser* m_peerView;
    QComboBox *m_linkAction,*m_linkRole;QGroupBox *m_receiveBox,*m_sendBox;QPushButton *m_receive,*m_send,*m_stopLink,*m_copyCode,*m_saveInvite;QLabel* m_linkRemaining;QTimer* m_linkCountdown;QElapsedTimer m_pairClock;bool m_linkBusy=false;
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
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    QApplication app(argc,argv);app.setOrganizationName("Veyon");app.setApplicationName("NetLinkCompanion");app.setApplicationDisplayName("NetLink Companion");app.setApplicationVersion(VEYON_VERSION);app.setWindowIcon(QIcon(":/companion/icon.png"));
    NetLinkAppearance::install();CompanionWindow window;window.show();new GuiLatencyMonitor(&app);return app.exec();
}
#endif
