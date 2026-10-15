// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025-26 prof. Ing. Raffaele Mele
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include "Diagnostics.h"
#include "NetLinkAppearance.h"
#include <veyonconfig.h>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
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
#include <QMainWindow>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSaveFile>
#include <QSpinBox>
#include <QTcpSocket>
#include <QTextBrowser>
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
        auto central=new QWidget(this);setCentralWidget(central);auto layout=new QVBoxLayout(central);
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
            render();
        });
        connect(m_export,&QPushButton::clicked,this,[this]{exportReport();});
        connect(m_compare,&QPushButton::clicked,this,[this]{compareReport();});
        connect(discovery,&QPushButton::clicked,this,[this]{m_port->setValue(3702);});
        m_socket=new QTcpSocket(this);m_probeDeadline=new QTimer(this);m_probeDeadline->setSingleShot(true);
        connect(m_probe,&QPushButton::clicked,this,[this]{
            const auto target=m_target->currentText().trimmed();if(!Companion::validTarget(target)){m_probeResult->setText("Inserire un solo nome PC o indirizzo IP, senza URL, CIDR o numero di porta.");return;}
            m_probe->setEnabled(false);m_probeResult->setText("Prova TCP in corso (massimo 8 secondi)…");m_probeDeadline->start(8000);m_socket->connectToHost(target,quint16(m_port->value()));
        });
        connect(m_socket,&QTcpSocket::connected,this,[this]{m_probeResult->setText(QString("TCP raggiungibile. Indirizzo locale usato: %1; destinatario effettivo: %2:%3. Autenticazione NON verificata.").arg(m_socket->localAddress().toString(),m_socket->peerAddress().toString()).arg(m_socket->peerPort()));finishProbe();});
        connect(m_socket,QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),this,[this](QAbstractSocket::SocketError){if(m_probe->isEnabled()) return;m_probeResult->setText("Collegamento non riuscito: "+m_socket->errorString()+". Questo non distingue da solo PC spento, porta errata, firewall o routing.");finishProbe();});
        connect(m_probeDeadline,&QTimer::timeout,this,[this]{m_probeResult->setText("Tempo scaduto. Verificare indirizzo, servizio, routing e firewall su entrambi i PC.");finishProbe();});
        if(!initialReport.isEmpty()) {m_report=initialReport;m_export->setEnabled(true);m_compare->setEnabled(true);render();}
        if(autoScan) QTimer::singleShot(0,this,[this]{scan();});
    }
protected:
    void closeEvent(QCloseEvent* event) override {
        if(m_collector->state()!=QProcess::NotRunning) {m_collector->kill();if(!m_collector->waitForFinished(1500)){event->ignore();m_status->setText("Attendere la chiusura del raccoglitore locale, poi riprovare.");return;}}
        finishProbe();event->accept();
    }
private:
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
