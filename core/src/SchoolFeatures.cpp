// SPDX-License-Identifier: GPL-2.0-or-later
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include "SchoolFeatures.h"
#include "SchoolNetworkPolicy.h"
#include "SchoolConfigurationPage.h"
#include "ConfigurationPage.h"
#include "Configuration/LocalStore.h"
#include "ConfigurationManager.h"
#include "CommandLineIO.h"
#include "FeatureWorkerManager.h"
#include "NetworkObject.h"
#include "VeyonConfiguration.h"
#include "VeyonMasterInterface.h"
#include "VeyonServerInterface.h"
#include "VeyonWorkerInterface.h"
#include <QtConcurrent>
#include <QDialogButtonBox>
#include <QCheckBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QSet>
#include <QHostInfo>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QTableWidget>
#include <QTextEdit>
#include <QTcpSocket>
#include <QVBoxLayout>

namespace {
class SchoolServiceThread : public QThread {
public:
    SchoolServiceThread(const QString& bootstrap,int port,bool discovery,QObject* parent)
        : QThread(parent),m_bootstrap(bootstrap),m_port(port),m_discovery(discovery) {}
    void run() override {
        QObject root;
        SchoolNetwork::PolicyDaemon policy(&root);
        if(m_discovery) new SchoolDiscovery::Agent(false,m_bootstrap,m_port,&root);
        exec();
    }
private:QString m_bootstrap;int m_port;bool m_discovery;
};
QString cleanName(const QString& input) {
    QString name=input.simplified();
    if(name.size()>120) return {};
    for(const auto c:name) if(c.category()==QChar::Other_Control || c.category()==QChar::Other_Format) return {};
    return name;
}
QString newId() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
QString addressText(QHostAddress address) {
    bool ipv4=false; const auto v4=address.toIPv4Address(&ipv4);
    if(ipv4) return QHostAddress(v4).toString();
    address.setScopeId({}); return address.toString();
}
QString humanStatus(const QJsonObject& result) {
    const auto status=result.value("status").toString();
    if(status=="blocked" && result.value("ok").toBool()) return "filtri attivi";
    if(status=="allowed" && result.value("ok").toBool()) return "libero";
    // WFP HRESULT values can exceed signed int; preserve the actual DWORD code.
    const auto code=result.value("code");
    return "errore: "+status+" ("+(code.isDouble() ? QString::number(code.toDouble(),'f',0) : "?")+")";
}
}
SchoolFeatures::SchoolFeatures(QObject* parent) : QObject(parent),
    m_guardUid("9509a4cc-f39e-4857-a3fc-8e288f1d577f"),
    m_attendanceUid("b74ef197-1db8-4f5b-b3aa-dc9e6eae843d"),
    m_discoveryUid("d0b09b52-c8ae-452b-b5a9-b5e2d612f89f"),m_timer(this) {
    const auto flags=Feature::Flag::Action|Feature::Flag::Master|Feature::Flag::Service|Feature::Flag::Worker|Feature::Flag::Builtin;
    m_features={Feature("InternetGuard",flags,m_guardUid,{},"InternetGuard","InternetGuard","Controllo WFP con feedback",":/core/veyon.png"),
        Feature("Appello",flags,m_attendanceUid,{},"Appello","Appello","Nomi dichiarati dagli studenti",":/core/veyon.png"),
        Feature("Discovery",Feature::Flag::Action|Feature::Flag::Master|Feature::Flag::Builtin,m_discoveryUid,{},"Trova client","Trova client","WS-Discovery 1.1",":/core/veyon.png")};
    m_clock.start();
    if(VeyonCore::component()==VeyonCore::Component::Service) {
        // The inherited Windows service dispatcher has no Qt main-thread event loop.
        m_serviceThread=new SchoolServiceThread(VeyonCore::config().schoolDiscoveryMaster(),VeyonCore::config().veyonServerPort(),VeyonCore::config().schoolDiscoveryEnabled(),this);
        m_serviceThread->start();
    }
    const bool master=VeyonCore::component()==VeyonCore::Component::Master;
    if(master && VeyonCore::config().schoolDiscoveryEnabled())
        m_discovery=new SchoolDiscovery::Agent(master,VeyonCore::config().schoolDiscoveryMaster(),VeyonCore::config().veyonServerPort(),this);
    if(master) {connect(&m_timer,&QTimer::timeout,this,&SchoolFeatures::tick);m_timer.start(1000);}
}
SchoolFeatures::~SchoolFeatures() {
    // The service lease, not GUI shutdown, is the recovery guarantee.
    if(m_studentDialog) delete m_studentDialog;
    if(m_serviceThread) {m_serviceThread->quit();m_serviceThread->wait();}
}
ConfigurationPage* SchoolFeatures::createConfigurationPage() {return new SchoolConfigurationPage(VeyonCore::config());}
CommandLinePluginInterface::RunResult SchoolFeatures::handle_firewall_setup(const QStringList&) {return SchoolNetwork::configureDiscoveryFirewall(true) ? CommandLinePluginInterface::Successful : CommandLinePluginInterface::Failed;}
CommandLinePluginInterface::RunResult SchoolFeatures::handle_firewall_remove(const QStringList&) {return SchoolNetwork::configureDiscoveryFirewall(false) ? CommandLinePluginInterface::Successful : CommandLinePluginInterface::Failed;}
CommandLinePluginInterface::RunResult SchoolFeatures::handle_selftest(const QStringList&) {
    const auto result=SchoolNetwork::nativeSelfTest();
    CommandLineIO::print(QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact)));
    return result.value("ok").toBool() ? CommandLinePluginInterface::Successful : CommandLinePluginInterface::Failed;
}
bool SchoolFeatures::controlFeature(Feature::Uid uid,Operation operation,const QVariantMap&,const ComputerControlInterfaceList& clients) {
    if(uid!=m_guardUid || operation==Operation::Initialize) return false;
    policy(clients,operation==Operation::Start);return true;
}
bool SchoolFeatures::startFeature(VeyonMasterInterface& master,const Feature& feature,const ComputerControlInterfaceList& clients) {
    if(feature.uid()==m_attendanceUid) {attendance(master,clients);return true;}
    if(feature.uid()==m_discoveryUid) {discovery(master);return true;}
    if(feature.uid()!=m_guardUid) return false;
    QDialog dialog(master.mainWindow());dialog.setWindowTitle("InternetGuard WFP");dialog.resize(760,400);
    auto layout=new QVBoxLayout(&dialog);
    auto label=new QLabel("WFP applicato ai client selezionati. I filtri scadono dopo 90 secondi senza rinnovo.\n"
        "Il feedback conferma i filtri installati, non una prova di navigazione Internet.",&dialog);
    label->setWordWrap(true);label->setTextFormat(Qt::PlainText);layout->addWidget(label);
    auto table=new QTableWidget(clients.size(),2,&dialog);table->setHorizontalHeaderLabels({"PC","Stato"});layout->addWidget(table);
    auto refresh=[table,clients]{for(int i=0;i<clients.size();++i){table->setItem(i,0,new QTableWidgetItem(clients[i]->computer().name()));table->setItem(i,1,new QTableWidgetItem(clients[i]->networkPolicyStatus()));}};
    QTimer timer;connect(&timer,&QTimer::timeout,&dialog,refresh);timer.start(1000);refresh();
    auto buttons=new QDialogButtonBox(&dialog);layout->addWidget(buttons);
    auto block=buttons->addButton("Blocca Internet",QDialogButtonBox::ActionRole);
    auto release=buttons->addButton("Sblocca",QDialogButtonBox::ActionRole);
    auto query=buttons->addButton("Verifica stato",QDialogButtonBox::ActionRole);
    connect(block,&QPushButton::clicked,&dialog,[this,clients]{policy(clients,true);});
    connect(release,&QPushButton::clicked,&dialog,[this,clients]{policy(clients,false);});
    connect(query,&QPushButton::clicked,&dialog,[this,clients]{for(const auto& ci:clients) sendPolicy(ci.data(),Status,{});});
    auto close=buttons->addButton(QDialogButtonBox::Close);connect(close,&QPushButton::clicked,&dialog,&QDialog::accept);
    dialog.exec();return true;
}
void SchoolFeatures::policy(const ComputerControlInterfaceList& clients,bool block) {
    const auto generation=++m_resolutionGeneration;
    if(!block) {
        for(const auto& ci:clients) {for(int i=m_guardTargets.size()-1;i>=0;--i) if(m_guardTargets[i].control==ci.data()) m_guardTargets.removeAt(i);sendPolicy(ci.data(),Release,{});}
        return;
    }
    if(m_resolving || clients.size()>256) return;
    m_resolving=true;
    QStringList hosts;
    for(const auto& ci:clients) hosts.append(ci->computer().hostAddress());
    const auto extras=VeyonCore::config().schoolResources();
    for(const auto& ci:clients) ci->setNetworkPolicyStatus("risoluzione PC...");
    auto resolver=new QObject(this);
    auto addresses=QSharedPointer<QStringList>::create(extras);
    auto outstanding=QSharedPointer<QSet<int>>::create();
    auto completed=QSharedPointer<bool>::create(false);
    auto finish=[this,resolver,addresses,outstanding,completed,clients,generation] {
        if(*completed) return;
        *completed=true;
        for(int id:*outstanding) QHostInfo::abortHostLookup(id);
        resolver->deleteLater();m_resolving=false;
        if(generation!=m_resolutionGeneration) return;
        addresses->removeDuplicates();
        if(!outstanding->isEmpty() || addresses->isEmpty() || addresses->size()>255) {for(const auto& ci:clients) ci->setNetworkPolicyStatus("errore: risoluzione incompleta o lista non valida");return;}
        for(const auto& ci:clients) {
            bool found=false;for(auto& target:m_guardTargets) if(target.control==ci.data()) {target.addresses=*addresses;target.sent=m_clock.elapsed();found=true;}
            if(!found && m_guardTargets.size()<256) m_guardTargets.append({ci.data(),*addresses,m_clock.elapsed()});
            sendPolicy(ci.data(),Apply,*addresses);
        }
    };
    hosts.removeDuplicates();
    for(const auto& host:hosts) {
        QHostAddress address(host);
        if(!address.isNull()) {if(SchoolNetwork::validAddress(addressText(address))) addresses->append(addressText(address));}
        else {
            const int id=QHostInfo::lookupHost(host,resolver,[addresses,outstanding,completed,finish](const QHostInfo& info) {
                if(*completed) return;
                outstanding->remove(info.lookupId());
                for(const auto& address:info.addresses()) if(SchoolNetwork::validAddress(addressText(address))) addresses->append(addressText(address));
                if(outstanding->isEmpty()) finish();
            });
            outstanding->insert(id);
        }
    }
    if(outstanding->isEmpty()) finish();
    else QTimer::singleShot(5000,resolver,finish);
}
void SchoolFeatures::sendPolicy(ComputerControlInterface* ci,int command,const QStringList& addresses) {
    if(!ci) return;
    if(ci->state()!=ComputerControlInterface::State::Connected) {ci->setNetworkPolicyStatus("non connesso");return;}
    const auto id=newId();ci->setProperty("schoolPolicyRequest",id);ci->setProperty("schoolPolicyDeadline",m_clock.elapsed()+8000);
    ci->setNetworkPolicyStatus("in attesa...");
    ci->sendFeatureMessage(FeatureMessage(m_guardUid,command).addArgument(0,id).addArgument(1,addresses));
    QPointer<ComputerControlInterface> guarded(ci);
    QTimer::singleShot(8000,this,[guarded,id] {if(guarded && guarded->property("schoolPolicyRequest").toString()==id && guarded->property("schoolPolicyDeadline").toLongLong()>0){guarded->setProperty("schoolPolicyDeadline",0);guarded->setNetworkPolicyStatus("nessuna conferma");}});
}
void SchoolFeatures::tick() {
    for(int i=m_guardTargets.size()-1;i>=0;--i) {
        auto& target=m_guardTargets[i];
        if(!target.control) {m_guardTargets.removeAt(i);continue;}
        auto ci=target.control.data();
        if(ci->property("schoolPolicyDeadline").toLongLong()>0 && ci->property("schoolPolicyDeadline").toLongLong()<m_clock.elapsed()) {ci->setProperty("schoolPolicyDeadline",0);ci->setNetworkPolicyStatus("nessuna conferma");}
        if(m_clock.elapsed()-target.sent>=20000) {target.sent=m_clock.elapsed();sendPolicy(ci,Apply,target.addresses);}
    }
}
bool SchoolFeatures::handleFeatureMessage(ComputerControlInterface::Pointer ci,const FeatureMessage& message) {
    if(message.featureUid()==m_guardUid && message.command()==Reply) {
        if(message.argument(0).toString()!=ci->property("schoolPolicyRequest").toString()) return true;
        ci->setProperty("schoolPolicyDeadline",0);
        ci->setNetworkPolicyStatus(humanStatus(QJsonDocument::fromJson(message.argument(1).toByteArray()).object()));return true;
    }
    if(message.featureUid()==m_attendanceUid && message.command()==NameReply) {
        if(message.argument(0).toString()!=ci->property("schoolAttendanceRequest").toString()) return true;
        ci->setStudentName(cleanName(message.argument(1).toString()));return true;
    }
    return false;
}
bool SchoolFeatures::handleFeatureMessage(VeyonServerInterface& server,const MessageContext& context,const FeatureMessage& message) {
    if(message.featureUid()==m_guardUid) {
        if(!context.connection() || !context.ioDevice() || message.command()<Apply || message.command()>Status) return true;
        if(context.ioDevice()->property("schoolPolicyBusy").toBool()) return true;
        const auto id=message.argument(0).toString();if(QUuid(id).isNull() || id.size()!=36) return true;
        QJsonObject command;command["op"]=message.command()==Apply ? "apply" : message.command()==Release ? "release" : "status";
        auto owner=context.ioDevice()->property("schoolPolicyOwner").toString();
        if(owner.isEmpty()) {owner=newId();context.ioDevice()->setProperty("schoolPolicyOwner",owner);}
        command["owner"]=owner;
        auto addresses=message.argument(1).toStringList();
        const auto socket=qobject_cast<QTcpSocket*>(context.ioDevice());
        if(!socket) return true;
        // Preserve the authenticated connection's actual peer, not a guessed LAN subnet.
        addresses.append(addressText(socket->peerAddress()));addresses.removeDuplicates();command["addresses"]=QJsonArray::fromStringList(addresses);
        context.ioDevice()->setProperty("schoolPolicyBusy",true);
        auto watcher=new QFutureWatcher<QJsonObject>(this);QPointer<VeyonServerInterface> guardedServer(&server);
        connect(watcher,&QFutureWatcher<QJsonObject>::finished,this,[this,watcher,context,guardedServer,id] {
            const auto result=watcher->result();watcher->deleteLater();
            if(context.ioDevice()) context.ioDevice()->setProperty("schoolPolicyBusy",false);
            if(guardedServer && context.ioDevice()) guardedServer->sendFeatureMessageReply(context,FeatureMessage(m_guardUid,Reply).addArgument(0,id).addArgument(1,QJsonDocument(result).toJson(QJsonDocument::Compact)));
        });
        watcher->setFuture(QtConcurrent::run([command]{return SchoolNetwork::request(command);}));return true;
    }
    if(message.featureUid()!=m_attendanceUid) return false;
    if(message.command()==NameReply && !context.connection()) {
        if(message.argument(0).toString()!=m_attendanceRequest || m_attendanceRequest.isEmpty()) return true;
        if(m_attendanceContext.ioDevice()) server.sendFeatureMessageReply(m_attendanceContext,FeatureMessage(m_attendanceUid,NameReply).addArgument(0,m_attendanceRequest).addArgument(1,cleanName(message.argument(1).toString())));
        m_attendanceRequest.clear();m_attendanceContext=MessageContext{};return true;
    }
    if(!context.connection() || !context.ioDevice()) return true;
    if(message.command()==AskName) {
        const auto request=message.argument(0).toString();if(request.size()!=36 || QUuid(request).isNull()) return true;
        if(m_attendanceContext.ioDevice() && m_attendanceContext.ioDevice()!=context.ioDevice()) return true;
        m_attendanceRequest=request;m_attendanceContext=context;
        server.featureWorkerManager().sendMessageToUnmanagedSessionWorker(message);
        QTimer::singleShot(180000,this,[this,request] {if(m_attendanceRequest==request){m_attendanceRequest.clear();m_attendanceContext=MessageContext{};}});
    } else if(message.command()==EndAttendance && (!m_attendanceContext.ioDevice() || m_attendanceContext.ioDevice()==context.ioDevice())) {
        m_attendanceRequest.clear();m_attendanceContext=MessageContext{};server.featureWorkerManager().stopWorker(m_attendanceUid);
    }
    return true;
}
bool SchoolFeatures::handleFeatureMessage(VeyonWorkerInterface& worker,const FeatureMessage& message) {
    if(message.featureUid()!=m_attendanceUid || message.command()!=AskName) return false;
    if(m_studentDialog) delete m_studentDialog;
    auto dialog=new QDialog;dialog->setAttribute(Qt::WA_DeleteOnClose);m_studentDialog=dialog;
    dialog->setWindowTitle("Appello — Veyon School");dialog->setWindowIcon(QIcon(":/core/veyon.png"));
    auto layout=new QVBoxLayout(dialog);auto label=new QLabel("Inserisci il tuo nome e cognome per l’appello del docente.",dialog);label->setTextFormat(Qt::PlainText);layout->addWidget(label);
    auto name=new QLineEdit(dialog);name->setMaxLength(120);layout->addWidget(name);
    auto buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,dialog);layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,dialog,[dialog,name] {if(!cleanName(name->text()).isEmpty()) dialog->accept();});
    connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::reject);
    const auto request=message.argument(0).toString();
    connect(dialog,&QDialog::finished,this,[this,&worker,name,request](int result) {worker.sendFeatureMessageReply(FeatureMessage(m_attendanceUid,NameReply).addArgument(0,request).addArgument(1,result==QDialog::Accepted ? cleanName(name->text()) : QString{}));});
    dialog->show();dialog->raise();dialog->activateWindow();name->setFocus();return true;
}
void SchoolFeatures::attendance(VeyonMasterInterface& master,const ComputerControlInterfaceList& clients) {
    QMessageBox box(QMessageBox::Question,"Appello","Il nome è dichiarato dallo studente, non verificato.\nScegli l’operazione per i PC selezionati.",QMessageBox::NoButton,master.mainWindow());box.setTextFormat(Qt::PlainText);
    auto ask=box.addButton("Richiedi nome",QMessageBox::ActionRole);auto end=box.addButton("Termina e cancella nomi",QMessageBox::ActionRole);auto exportButton=box.addButton("Esporta CSV",QMessageBox::ActionRole);box.addButton(QMessageBox::Cancel);box.exec();
    if(box.clickedButton()==ask) {
        for(const auto& ci:clients) {const auto id=newId();ci->setProperty("schoolAttendanceRequest",id);ci->setStudentName({});ci->sendFeatureMessage(FeatureMessage(m_attendanceUid,AskName).addArgument(0,id));}
    } else if(box.clickedButton()==end) {
        for(const auto& ci:clients) {ci->setProperty("schoolAttendanceRequest",QString{});ci->setStudentName({});ci->sendFeatureMessage(FeatureMessage(m_attendanceUid,EndAttendance));}
    } else if(box.clickedButton()==exportButton) {
        const auto path=QFileDialog::getSaveFileName(master.mainWindow(),"Esporta appello",{},"CSV (*.csv)");if(path.isEmpty()) return;
        auto quote=[](QString text){const auto trimmed=text.trimmed();if(!trimmed.isEmpty() && QString("=+-@").contains(trimmed.front())) text.prepend('\'');text.replace('"',"\"\"");return '"'+text+'"';};
        QByteArray data("\xef\xbb\xbfPC;Studente;Connessione\r\n");
        for(const auto& ci:clients) data+=(quote(ci->computer().name())+';'+quote(ci->studentName())+';'+quote(ci->state()==ComputerControlInterface::State::Connected ? "online" : "offline")+"\r\n").toUtf8();
        QSaveFile file(path);if(!file.open(QIODevice::WriteOnly) || file.write(data)!=data.size() || !file.commit()) QMessageBox::warning(master.mainWindow(),"Appello","Esportazione non riuscita.");
    }
}
void SchoolFeatures::discovery(VeyonMasterInterface& master) {
    if(!m_discovery) {QMessageBox::information(master.mainWindow(),"WS-Discovery","Discovery disabilitato nel Configurator.");return;}
    m_discovery->probe();
    QDialog dialog(master.mainWindow());dialog.setWindowTitle("Client trovati — WS-Discovery");dialog.resize(800,400);
    auto layout=new QVBoxLayout(&dialog);auto help=new QLabel("Gli annunci non sono autenticati. Verifica i PC prima di importarli.\nPer altre subnet distribuisci School/DiscoveryMaster ai client. "+m_discovery->error(),&dialog);help->setWordWrap(true);help->setTextFormat(Qt::PlainText);layout->addWidget(help);
    auto table=new QTableWidget(&dialog);table->setColumnCount(3);table->setHorizontalHeaderLabels({"Nome dichiarato dal PC","IP rilevato","Porta"});table->setSelectionBehavior(QAbstractItemView::SelectRows);layout->addWidget(table);
    QList<SchoolDiscovery::Record> records;
    auto refresh=[this,table,&records] {records=m_discovery->records();table->setRowCount(records.size());for(int i=0;i<records.size();++i){table->setItem(i,0,new QTableWidgetItem(records[i].name));table->setItem(i,1,new QTableWidgetItem(records[i].address));table->setItem(i,2,new QTableWidgetItem(QString::number(records[i].port)));}};
    QTimer timer;connect(&timer,&QTimer::timeout,&dialog,refresh);timer.start(2000);refresh();
    auto buttons=new QDialogButtonBox(QDialogButtonBox::Close,&dialog);layout->addWidget(buttons);
    auto import=buttons->addButton("Importa PC selezionato",QDialogButtonBox::ActionRole);
    connect(import,&QPushButton::clicked,&dialog,[this,&dialog,table,&records] {
        const int row=table->currentRow();if(row<0 || row>=records.size()) return;
        const auto record=records[row];
        if(record.port!=VeyonCore::config().veyonServerPort()) {QMessageBox::warning(&dialog,"Discovery","Porta diversa da quella configurata: aggiungi il PC manualmente.");return;}
        if(QMessageBox::question(&dialog,"Importa PC",record.name+" — "+record.address+"\nConfermi che appartiene alla scuola?")!=QMessageBox::Yes) return;
        auto& config=VeyonCore::config();
        if(!Configuration::LocalStore(Configuration::LocalStore::System).isWritable()) {
            QMessageBox::warning(&dialog,"Discovery","Occorrono diritti amministrativi per importare un PC nella configurazione.");return;
        }
        auto objects=config.value("NetworkObjects","BuiltinDirectory",QJsonArray{}).toJsonArray();
        QUuid location;
        for(const auto& value:objects) {const NetworkObject object(value.toObject());if(object.type()==NetworkObject::Type::Host && object.hostAddress()==record.address) {QMessageBox::information(&dialog,"Discovery","PC già configurato.");return;}if(object.type()==NetworkObject::Type::Location && location.isNull()) location=object.uid();}
        if(location.isNull()) {location=QUuid::createUuid();objects.append(NetworkObject(NetworkObject::Type::Location,"Discovery",{},{},{},location).toJson());}
        objects.append(NetworkObject(NetworkObject::Type::Host,record.name.left(120),record.address,{},{},QUuid::createUuid(),location).toJson());
        config.setValue("NetworkObjects",objects,"BuiltinDirectory");
        ConfigurationManager manager;
        if(!manager.saveConfiguration()) {QMessageBox::warning(&dialog,"Discovery",manager.errorString());return;}
        QMessageBox::information(&dialog,"Discovery","PC importato. Riavvia il Master per visualizzarlo; l’autenticazione Veyon rimane necessaria.");
    });
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);dialog.exec();
}
