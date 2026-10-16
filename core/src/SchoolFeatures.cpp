// SPDX-License-Identifier: GPL-2.0-or-later
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include "SchoolFeatures.h"
#include "SchoolNetworkPolicy.h"
#include "SchoolPolicyResolver.h"
#include "SchoolConfigurationPage.h"
#include "NetLinkUi.h"
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
#include <QHeaderView>
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
#include <algorithm>

namespace {
class SchoolServiceThread : public QThread {
public:
    SchoolServiceThread(const QString& bootstrap,int port,bool discovery,QObject* parent)
        : QThread(parent),m_bootstrap(bootstrap),m_port(port),m_discovery(discovery) {}
    void run() override {
        if(isInterruptionRequested()) return;
        QObject root;
        SchoolNetwork::PolicyDaemon policy(&root);
        if(m_discovery) new SchoolDiscovery::Agent(false,m_bootstrap,m_port,&root);
        if(!isInterruptionRequested()) exec();
    }
private:QString m_bootstrap;int m_port;bool m_discovery;
};
QString cleanName(const QString& input) {
    if(input.size()>1024) return {};
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
    const auto status=result.value("status").toString().left(160);
    if(status=="blocked" && result.value("ok").toBool()) return NetLinkUi::text("filters active");
    if(status=="allowed" && result.value("ok").toBool()) return NetLinkUi::text("unblocked");
    // WFP HRESULT values can exceed signed int; preserve the actual DWORD code.
    const auto code=result.value("code");
    return NetLinkUi::text("error: ")+status+" ("+(code.isDouble() ? QString::number(code.toDouble(),'f',0) : "?")+")";
}
bool bringForward(QPointer<QDialog> dialog) {
    if(!dialog || !dialog->isVisible()) return false;
    dialog->raise(); dialog->activateWindow(); return true;
}
void prepareDialog(QDialog* dialog, const QString& objectName) {
    dialog->setObjectName(objectName);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowModality(Qt::NonModal);
    dialog->setWindowIcon(QIcon(":/core/veyon.png"));
    QTimer::singleShot(0,dialog,[dialog] {
        dialog->setWindowTitle(NetLinkUi::windowTitle(dialog->windowTitle()));
    });
}
void notify(QWidget* parent, QMessageBox::Icon icon, const QString& title, const QString& text) {
    auto box=new QMessageBox(icon,NetLinkUi::windowTitle(title),text,QMessageBox::Ok,parent);
    box->setTextFormat(Qt::PlainText);box->setAttribute(Qt::WA_DeleteOnClose);box->open();
}
}
SchoolFeatures::SchoolFeatures(QObject* parent) : SchoolFeatures(parent,true) {}
SchoolFeatures::SchoolFeatures(QObject* parent,bool initializeRuntime) : QObject(parent),
    m_guardUid("9509a4cc-f39e-4857-a3fc-8e288f1d577f"),
    m_attendanceUid("b74ef197-1db8-4f5b-b3aa-dc9e6eae843d"),
    m_discoveryUid("d0b09b52-c8ae-452b-b5a9-b5e2d612f89f"),m_timer(this),m_attendanceTimer(this) {
    const auto flags=Feature::Flag::Action|Feature::Flag::Master|Feature::Flag::Service|Feature::Flag::Worker|Feature::Flag::Builtin;
    m_features={Feature("InternetGuard",flags,m_guardUid,{},"InternetGuard","InternetGuard",NetLinkUi::text("WFP controls with client feedback"),":/core/veyon.png"),
        Feature("Appello",flags,m_attendanceUid,{},NetLinkUi::text("Attendance"),NetLinkUi::text("Attendance"),NetLinkUi::text("Self-declared student names"),":/core/veyon.png"),
        Feature("Discovery",Feature::Flag::Action|Feature::Flag::Master|Feature::Flag::Builtin,m_discoveryUid,{},NetLinkUi::text("Find clients"),NetLinkUi::text("Find clients"),"WS-Discovery 1.1",":/core/veyon.png")};
    m_clock.start();
    m_attendanceTimer.setSingleShot(true);m_attendanceTimer.setInterval(180000);
    connect(&m_attendanceTimer,&QTimer::timeout,this,[this] {m_attendanceRequest.clear();m_attendanceContext=MessageContext{};});
    if(!initializeRuntime) return;
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
    if(m_guardDialog) delete m_guardDialog;
    if(m_attendanceDialog) delete m_attendanceDialog;
    if(m_discoveryDialog) delete m_discoveryDialog;
    if(m_resolution) {m_resolution->cancel();delete m_resolution;}
    if(m_serviceThread) {
        m_serviceThread->requestInterruption();m_serviceThread->quit();
        // Never terminate a WFP transaction or destroy a running QThread. Native
        // RPC teardown can still be slow; record it instead of hiding the stall.
        if(!m_serviceThread->wait(5000)) {
            qCritical("NetLink school service shutdown exceeded 5 seconds; waiting for safe native cleanup");
            m_serviceThread->wait();
        }
    }
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
    if(bringForward(m_guardDialog)) return true;
    auto dialog=new QDialog(master.mainWindow());m_guardDialog=dialog;
    prepareDialog(dialog,"schoolInternetGuardDialog");dialog->setWindowTitle("InternetGuard WFP");dialog->resize(760,400);
    auto layout=new QVBoxLayout(dialog);
    auto label=new QLabel(NetLinkUi::text("WFP applies to selected clients. Filters expire after 90 seconds without renewal.\nFeedback confirms filter installation, not successful Internet-blocking tests."),dialog);
    label->setWordWrap(true);label->setTextFormat(Qt::PlainText);layout->addWidget(label);
    auto table=new QTableWidget(clients.size(),2,dialog);table->setHorizontalHeaderLabels({NetLinkUi::text("Computers selected when opened"),NetLinkUi::text("Status")});layout->addWidget(table);
    for(int i=0;i<clients.size();++i) {
        const auto ci=clients[i];
        table->setItem(i,0,new QTableWidgetItem(ci ? ci->computer().name() : QString{}));
        auto status=new QTableWidgetItem(ci ? ci->networkPolicyStatus() : QString(NetLinkUi::text("unavailable")));table->setItem(i,1,status);
        if(ci) connect(ci.data(),&ComputerControlInterface::userChanged,dialog,[ci,status] {status->setText(ci->networkPolicyStatus());});
    }
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);table->horizontalHeader()->setStretchLastSection(true);
    auto buttons=new QDialogButtonBox(dialog);layout->addWidget(buttons);
    auto block=buttons->addButton(NetLinkUi::text("Block Internet"),QDialogButtonBox::ActionRole);
    auto release=buttons->addButton(NetLinkUi::text("Unblock"),QDialogButtonBox::ActionRole);
    auto query=buttons->addButton(NetLinkUi::text("Check status"),QDialogButtonBox::ActionRole);
    connect(block,&QPushButton::clicked,dialog,[this,clients]{policy(clients,true);});
    connect(release,&QPushButton::clicked,dialog,[this,clients]{policy(clients,false);});
    connect(query,&QPushButton::clicked,dialog,[this,clients]{for(const auto& ci:clients) sendPolicy(ci.data(),Status,{});});
    auto close=buttons->addButton(QDialogButtonBox::Close);connect(close,&QPushButton::clicked,dialog,&QDialog::accept);
    // Closing this monitor never releases a policy: its authenticated explicit
    // Release command and service-side lease remain the only recovery paths.
    dialog->show();return true;
}
void SchoolFeatures::policy(const ComputerControlInterfaceList& clients,bool block) {
    if(!block) {
        bool overlaps=false;
        for(const auto& ci:clients) if(m_resolutionClients.contains(ci)) overlaps=true;
        if(overlaps && m_resolution) {
            m_resolution->cancel();m_resolution->deleteLater();m_resolution=nullptr;
            for(const auto& ci:m_resolutionClients) if(ci) ci->setNetworkPolicyStatus(NetLinkUi::text("resolution cancelled; no new filters applied"));
            m_resolutionClients.clear();
        }
        for(const auto& ci:clients) {for(int i=m_guardTargets.size()-1;i>=0;--i) if(m_guardTargets[i].control==ci.data()) m_guardTargets.removeAt(i);sendPolicy(ci.data(),Release,{});}
        return;
    }
    // A second press must not invalidate the batch already in flight.
    if(m_resolution) {qWarning("NetLink policy DNS request already in progress");return;}
    beginPolicyResolution(clients,VeyonCore::config().schoolResources());
}
void SchoolFeatures::beginPolicyResolution(const ComputerControlInterfaceList& clients,const QStringList& resources) {
    if(m_resolution || clients.isEmpty() || clients.size()>256) return;
    QStringList hosts;
    for(const auto& ci:clients) {if(!ci) return;hosts.append(ci->computer().hostAddress());}
    auto resolver=new SchoolPolicyResolver(hosts,resources,this);m_resolution=resolver;m_resolutionClients=clients;
    for(const auto& ci:clients) ci->setNetworkPolicyStatus(NetLinkUi::text("resolving computers..."));
    connect(resolver,&SchoolPolicyResolver::finished,this,[this,resolver,clients](bool complete,const QStringList& addresses) {
        resolver->deleteLater();
        if(m_resolution!=resolver) return;
        m_resolution=nullptr;m_resolutionClients.clear();
        if(!complete) {for(const auto& ci:clients) ci->setNetworkPolicyStatus(NetLinkUi::text("error: incomplete resolution or invalid list"));return;}
        for(const auto& ci:clients) {
            if(ci->state()!=ComputerControlInterface::State::Connected) {ci->setNetworkPolicyStatus(NetLinkUi::text("not connected; no new filters applied"));continue;}
            bool found=false;for(auto& target:m_guardTargets) if(target.control==ci.data()) {target.addresses=addresses;target.sent=m_clock.elapsed();found=true;}
            if(!found) {
                if(m_guardTargets.size()>=256) {ci->setNetworkPolicyStatus(NetLinkUi::text("InternetGuard client limit reached"));continue;}
                m_guardTargets.append({ci.data(),addresses,m_clock.elapsed()});
            }
            sendPolicy(ci.data(),Apply,addresses);
        }
    });
    resolver->start();
}
void SchoolFeatures::sendPolicy(ComputerControlInterface* ci,int command,const QStringList& addresses,int retry) {
    if(!ci) return;
    if(ci->state()!=ComputerControlInterface::State::Connected) {ci->setNetworkPolicyStatus(NetLinkUi::text("not connected"));return;}
    if(command==Status && ci->property("schoolPolicyDeadline").toLongLong()>m_clock.elapsed()) return;
    const auto id=newId();ci->setProperty("schoolPolicyRequest",id);ci->setProperty("schoolPolicyDeadline",m_clock.elapsed()+8000);
    ci->setProperty("schoolPolicyCommand",command);ci->setProperty("schoolPolicyRetry",retry);
    ci->setNetworkPolicyStatus(NetLinkUi::text("pending..."));
    ci->sendFeatureMessage(FeatureMessage(m_guardUid,command).addArgument(0,id).addArgument(1,addresses));
    QPointer<ComputerControlInterface> guarded(ci);
    auto deadline=ci->findChild<QTimer*>("schoolPolicyDeadlineTimer",Qt::FindDirectChildrenOnly);
    if(!deadline) {deadline=new QTimer(ci);deadline->setObjectName("schoolPolicyDeadlineTimer");deadline->setSingleShot(true);}
    disconnect(deadline,nullptr,this,nullptr);
    connect(deadline,&QTimer::timeout,this,[guarded,id] {if(guarded && guarded->property("schoolPolicyRequest").toString()==id && guarded->property("schoolPolicyDeadline").toLongLong()>0){guarded->setProperty("schoolPolicyDeadline",0);guarded->setNetworkPolicyStatus(NetLinkUi::text("no confirmation"));}});deadline->start(8000);
}
void SchoolFeatures::tick() {
    for(int i=m_guardTargets.size()-1;i>=0;--i) {
        auto& target=m_guardTargets[i];
        if(!target.control) {m_guardTargets.removeAt(i);continue;}
        auto ci=target.control.data();
        if(ci->property("schoolPolicyDeadline").toLongLong()>0 && ci->property("schoolPolicyDeadline").toLongLong()<m_clock.elapsed()) {ci->setProperty("schoolPolicyDeadline",0);ci->setNetworkPolicyStatus(NetLinkUi::text("no confirmation"));}
        if(m_clock.elapsed()-target.sent>=20000) {target.sent=m_clock.elapsed();sendPolicy(ci,Apply,target.addresses);}
    }
}
bool SchoolFeatures::handleFeatureMessage(ComputerControlInterface::Pointer ci,const FeatureMessage& message) {
    if(!ci) return false;
    if(message.featureUid()==m_guardUid && message.command()==Reply) {
        const auto request=ci->property("schoolPolicyRequest").toString();
        if(request.isEmpty() || message.argument(0).toString()!=request) return true;
        const auto data=message.argument(1).toByteArray();
        if(data.isEmpty() || data.size()>32768) {ci->setNetworkPolicyStatus(NetLinkUi::text("invalid InternetGuard reply"));return true;}
        QJsonParseError error;
        const auto document=QJsonDocument::fromJson(data,&error);
        if(error.error!=QJsonParseError::NoError || !document.isObject()) {ci->setNetworkPolicyStatus(NetLinkUi::text("invalid InternetGuard reply"));return true;}
        const auto result=document.object();
        ci->setProperty("schoolPolicyDeadline",0);
        if(result.value("status").toString()=="busy" && ci->property("schoolPolicyCommand").toInt()==Release && ci->property("schoolPolicyRetry").toInt()<2) {
            const auto id=message.argument(0).toString();const int retry=ci->property("schoolPolicyRetry").toInt()+1;
            ci->setNetworkPolicyStatus(NetLinkUi::text("service busy; retrying unblock..."));
            QTimer::singleShot(750,this,[this,ci,id,retry] {if(ci->property("schoolPolicyRequest").toString()==id) sendPolicy(ci.data(),Release,{},retry);});
        } else ci->setNetworkPolicyStatus(humanStatus(result));return true;
    }
    if(message.featureUid()==m_attendanceUid && message.command()==NameReply) {
        const auto request=ci->property("schoolAttendanceRequest").toString();
        if(request.isEmpty() || message.argument(0).toString()!=request) return true;
        ci->setProperty("schoolAttendanceRequest",QString{});
        ci->setStudentName(cleanName(message.argument(1).toString()));return true;
    }
    return false;
}
bool SchoolFeatures::handleFeatureMessage(VeyonServerInterface& server,const MessageContext& context,const FeatureMessage& message) {
    if(message.featureUid()==m_guardUid) {
        if(!context.connection() || !context.ioDevice() || message.command()<Apply || message.command()>Status) return true;
        const auto id=message.argument(0).toString();if(QUuid(id).isNull() || id.size()!=36) return true;
        if(context.ioDevice()->property("schoolPolicyBusy").toBool()) {
            // Preserve only the newest explicit Release while this authenticated
            // connection is applying a policy. Never queue attacker-sized work.
            if(message.command()==Release) context.ioDevice()->setProperty("schoolPolicyPendingRelease",id);
            else server.sendFeatureMessageReply(context,FeatureMessage(m_guardUid,Reply).addArgument(0,id).addArgument(1,QJsonDocument(QJsonObject{{"ok",false},{"status","busy"}}).toJson(QJsonDocument::Compact)));
            return true;
        }
        if(m_policyInFlight>=8) {
            server.sendFeatureMessageReply(context,FeatureMessage(m_guardUid,Reply).addArgument(0,id).addArgument(1,QJsonDocument(QJsonObject{{"ok",false},{"status","busy"}}).toJson(QJsonDocument::Compact)));return true;
        }
        QJsonObject command;command["op"]=message.command()==Apply ? "apply" : message.command()==Release ? "release" : "status";
        auto owner=context.ioDevice()->property("schoolPolicyOwner").toString();
        if(owner.isEmpty()) {owner=newId();context.ioDevice()->setProperty("schoolPolicyOwner",owner);}
        command["owner"]=owner;
        auto addresses=message.command()==Apply ? message.argument(1).toStringList() : QStringList{};
        if(addresses.size()>255 || std::any_of(addresses.cbegin(),addresses.cend(),[](const QString& address) {return address.size()>128;})) {
            server.sendFeatureMessageReply(context,FeatureMessage(m_guardUid,Reply).addArgument(0,id).addArgument(1,QJsonDocument(QJsonObject{{"ok",false},{"status","invalid-request"}}).toJson(QJsonDocument::Compact)));return true;
        }
        const auto socket=qobject_cast<QTcpSocket*>(context.ioDevice());
        if(!socket) return true;
        // Preserve the authenticated connection's actual peer, not a guessed LAN subnet.
        addresses.append(addressText(socket->peerAddress()));addresses.removeDuplicates();command["addresses"]=QJsonArray::fromStringList(addresses);
        context.ioDevice()->setProperty("schoolPolicyBusy",true);
        ++m_policyInFlight;
        auto watcher=new QFutureWatcher<QJsonObject>(this);QPointer<VeyonServerInterface> guardedServer(&server);
        connect(watcher,&QFutureWatcher<QJsonObject>::finished,this,[this,watcher,context,guardedServer,id] {
            const auto result=watcher->result();watcher->deleteLater();
            --m_policyInFlight;
            if(context.ioDevice()) context.ioDevice()->setProperty("schoolPolicyBusy",false);
            if(guardedServer && context.ioDevice()) guardedServer->sendFeatureMessageReply(context,FeatureMessage(m_guardUid,Reply).addArgument(0,id).addArgument(1,QJsonDocument(result).toJson(QJsonDocument::Compact)));
            if(guardedServer && context.ioDevice() && context.connection()) {
                const auto releaseId=context.ioDevice()->property("schoolPolicyPendingRelease").toString();
                context.ioDevice()->setProperty("schoolPolicyPendingRelease",QString{});
                if(!releaseId.isEmpty()) handleFeatureMessage(*guardedServer,context,FeatureMessage(m_guardUid,Release).addArgument(0,releaseId));
            }
        });
        watcher->setFuture(QtConcurrent::run([command]{return SchoolNetwork::request(command);}));return true;
    }
    if(message.featureUid()!=m_attendanceUid) return false;
    if(message.command()==NameReply && !context.connection()) {
        if(message.argument(0).toString()!=m_attendanceRequest || m_attendanceRequest.isEmpty()) return true;
        if(m_attendanceContext.ioDevice()) server.sendFeatureMessageReply(m_attendanceContext,FeatureMessage(m_attendanceUid,NameReply).addArgument(0,m_attendanceRequest).addArgument(1,cleanName(message.argument(1).toString())));
        m_attendanceTimer.stop();m_attendanceRequest.clear();m_attendanceContext=MessageContext{};return true;
    }
    if(!context.connection() || !context.ioDevice()) return true;
    if(message.command()==AskName) {
        const auto request=message.argument(0).toString();if(request.size()!=36 || QUuid(request).isNull()) return true;
        if(m_attendanceContext.ioDevice() && m_attendanceContext.ioDevice()!=context.ioDevice()) return true;
        m_attendanceRequest=request;m_attendanceContext=context;
        server.featureWorkerManager().sendMessageToUnmanagedSessionWorker(message);
        m_attendanceTimer.start();
    } else if(message.command()==EndAttendance && (!m_attendanceContext.ioDevice() || m_attendanceContext.ioDevice()==context.ioDevice())) {
        m_attendanceTimer.stop();m_attendanceRequest.clear();m_attendanceContext=MessageContext{};server.featureWorkerManager().stopWorker(m_attendanceUid);
    }
    return true;
}
bool SchoolFeatures::handleFeatureMessage(VeyonWorkerInterface& worker,const FeatureMessage& message) {
    if(message.featureUid()!=m_attendanceUid || message.command()!=AskName) return false;
    const auto request=message.argument(0).toString();
    if(request.size()!=36 || QUuid(request).isNull()) return true;
    // A worker is QObject-backed in production. Do not keep an unguarded
    // reference across asynchronous dialog completion or worker teardown.
    auto workerObject=dynamic_cast<QObject*>(&worker);
    if(!workerObject) {qWarning("NetLink attendance worker has no guarded QObject lifetime");return true;}
    if(m_studentDialog && m_studentDialog->property("schoolAttendanceRequest").toString()==request && bringForward(m_studentDialog)) return true;
    if(m_studentDialog) delete m_studentDialog;
    auto dialog=new QDialog;dialog->setAttribute(Qt::WA_DeleteOnClose);
    // Closing this one-shot UI must not terminate the event loop before the
    // worker's TCP reply reaches the service. The worker owns bounded shutdown.
    dialog->setAttribute(Qt::WA_QuitOnClose,false);m_studentDialog=dialog;
    dialog->setProperty("schoolAttendanceRequest",request);
    dialog->setWindowTitle(NetLinkUi::windowTitle(NetLinkUi::text("Attendance — NetLink")));dialog->setWindowIcon(QIcon(":/core/veyon.png"));
    auto layout=new QVBoxLayout(dialog);auto label=new QLabel(NetLinkUi::text("Enter your first and last name for the teacher's attendance request."),dialog);label->setTextFormat(Qt::PlainText);layout->addWidget(label);
    auto name=new QLineEdit(dialog);name->setMaxLength(120);layout->addWidget(name);
    auto buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,dialog);layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,dialog,[dialog,name] {if(!cleanName(name->text()).isEmpty()) dialog->accept();});
    connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::reject);
    QPointer<QObject> guardedWorker(workerObject);
    connect(workerObject,&QObject::destroyed,dialog,&QObject::deleteLater);
    connect(dialog,&QDialog::finished,this,[this,guardedWorker,name,request](int result) {
        if(auto live=dynamic_cast<VeyonWorkerInterface*>(guardedWorker.data())) {
            if(!live->sendFeatureMessageReply(FeatureMessage(m_attendanceUid,NameReply).addArgument(0,request).addArgument(1,result==QDialog::Accepted ? cleanName(name->text()) : QString{})))
                qWarning("NetLink attendance reply could not be queued");
            // A transport implementation may synchronously destroy its owner.
            if(auto remaining=dynamic_cast<VeyonWorkerInterface*>(guardedWorker.data())) remaining->finishAfterReplies();
        }
    });
    QTimer::singleShot(180000,dialog,&QDialog::reject);
    dialog->show();dialog->raise();dialog->activateWindow();name->setFocus();return true;
}
void SchoolFeatures::attendance(VeyonMasterInterface& master,const ComputerControlInterfaceList& clients) {
    if(bringForward(m_attendanceDialog)) return;
    auto box=new QMessageBox(QMessageBox::Question,NetLinkUi::text("Attendance"),NetLinkUi::text("Names are self-declared, not verified.\nChoose an operation for the computers selected when opened."),QMessageBox::NoButton,master.mainWindow());
    m_attendanceDialog=box;prepareDialog(box,"schoolAttendanceDialog");box->setTextFormat(Qt::PlainText);
    auto ask=box->addButton(NetLinkUi::text("Request names"),QMessageBox::ActionRole);auto end=box->addButton(NetLinkUi::text("End and clear names"),QMessageBox::ActionRole);auto exportButton=box->addButton(NetLinkUi::text("Export CSV"),QMessageBox::ActionRole);box->addButton(QMessageBox::Cancel);
    QPointer<QWidget> parent(master.mainWindow());
    connect(box,&QMessageBox::buttonClicked,this,[this,box,ask,end,exportButton,clients,parent](QAbstractButton* clicked) {
        if(clicked==ask) {
            for(const auto& ci:clients) if(ci) {
                if(ci->state()!=ComputerControlInterface::State::Connected) continue;
                const auto id=newId();ci->setProperty("schoolAttendanceRequest",id);ci->setStudentName({});ci->sendFeatureMessage(FeatureMessage(m_attendanceUid,AskName).addArgument(0,id));
                QPointer<ComputerControlInterface> guarded(ci.data());
                auto deadline=ci->findChild<QTimer*>("schoolAttendanceDeadlineTimer",Qt::FindDirectChildrenOnly);
                if(!deadline) {deadline=new QTimer(ci.data());deadline->setObjectName("schoolAttendanceDeadlineTimer");deadline->setSingleShot(true);}
                disconnect(deadline,nullptr,this,nullptr);
                connect(deadline,&QTimer::timeout,this,[guarded,id] {if(guarded && guarded->property("schoolAttendanceRequest").toString()==id) guarded->setProperty("schoolAttendanceRequest",QString{});});deadline->start(180000);
            }
        } else if(clicked==end) {
            for(const auto& ci:clients) if(ci) {ci->setProperty("schoolAttendanceRequest",QString{});ci->setStudentName({});ci->sendFeatureMessage(FeatureMessage(m_attendanceUid,EndAttendance));}
        } else if(clicked==exportButton && parent) {
            auto chooser=new QFileDialog(parent,NetLinkUi::text("Export attendance"),{},"CSV (*.csv)");
            chooser->setAcceptMode(QFileDialog::AcceptSave);chooser->setDefaultSuffix("csv");chooser->setAttribute(Qt::WA_DeleteOnClose);
            connect(chooser,&QFileDialog::fileSelected,this,[clients,parent](const QString& path) {
                if(path.isEmpty()) return;
                auto quote=[](QString text){const auto trimmed=text.trimmed();if(!trimmed.isEmpty() && QString("=+-@").contains(trimmed.front())) text.prepend('\'');text.replace('"',"\"\"");return '"'+text+'"';};
                QByteArray data("\xef\xbb\xbf");data+=NetLinkUi::text("PC;Student;Connection\r\n").toUtf8();
                for(const auto& ci:clients) if(ci) data+=(quote(ci->computer().name())+';'+quote(ci->studentName())+';'+quote(ci->state()==ComputerControlInterface::State::Connected ? "online" : "offline")+"\r\n").toUtf8();
                QSaveFile file(path);if(!file.open(QIODevice::WriteOnly) || file.write(data)!=data.size() || !file.commit()) notify(parent,QMessageBox::Warning,NetLinkUi::text("Attendance"),NetLinkUi::text("Export failed."));
            });
            chooser->open();
        }
        Q_UNUSED(box)
    });
    box->show();
}
void SchoolFeatures::discovery(VeyonMasterInterface& master) {
    if(bringForward(m_discoveryDialog)) return;
    if(!m_discovery) {
        auto box=new QMessageBox(QMessageBox::Information,"WS-Discovery",NetLinkUi::text("Discovery disabled in Configurator."),QMessageBox::Ok,master.mainWindow());
        m_discoveryDialog=box;prepareDialog(box,"schoolDiscoveryDialog");box->setTextFormat(Qt::PlainText);box->show();return;
    }
    m_discovery->probe();
    auto dialog=new QDialog(master.mainWindow());m_discoveryDialog=dialog;prepareDialog(dialog,"schoolDiscoveryDialog");dialog->setWindowTitle(NetLinkUi::text("Discovered clients — WS-Discovery"));dialog->resize(800,400);
    auto layout=new QVBoxLayout(dialog);auto help=new QLabel(NetLinkUi::text("Only remote computers with IPv4 addresses are shown; this computer is excluded.\nAnnouncements are not authenticated. Verify computers before importing them.\nFor other subnets, distribute School/DiscoveryMaster to clients. ")+m_discovery->error(),dialog);help->setWordWrap(true);help->setTextFormat(Qt::PlainText);layout->addWidget(help);
    auto table=new QTableWidget(dialog);table->setColumnCount(3);table->setHorizontalHeaderLabels({NetLinkUi::text("Name reported by computer"),NetLinkUi::text("Detected IPv4"),NetLinkUi::text("Port")});table->setSelectionBehavior(QAbstractItemView::SelectRows);table->setSelectionMode(QAbstractItemView::SingleSelection);table->setEditTriggers(QAbstractItemView::NoEditTriggers);layout->addWidget(table);
    auto records=QSharedPointer<QList<SchoolDiscovery::Record>>::create();
    auto refresh=[this,table,records] {
        auto updated=m_discovery->records();
        std::sort(updated.begin(),updated.end(),[](const SchoolDiscovery::Record& a,const SchoolDiscovery::Record& b) {return a.endpoint<b.endpoint;});
        bool changed=updated.size()!=records->size();
        if(!changed) for(int i=0;i<updated.size();++i) {
            const auto& a=updated[i];const auto& b=(*records)[i];
            if(a.endpoint!=b.endpoint || a.name!=b.name || a.address!=b.address || a.port!=b.port) {changed=true;break;}
        }
        if(!changed) return;
        const int selected=table->currentRow();const auto endpoint=selected>=0 && selected<records->size() ? (*records)[selected].endpoint : QString{};
        *records=updated;table->setUpdatesEnabled(false);table->clearSelection();table->setCurrentItem(nullptr);table->setRowCount(records->size());
        for(int i=0;i<records->size();++i) {const auto& record=(*records)[i];table->setItem(i,0,new QTableWidgetItem(record.name));table->setItem(i,1,new QTableWidgetItem(record.address));table->setItem(i,2,new QTableWidgetItem(QString::number(record.port)));if(record.endpoint==endpoint) table->selectRow(i);}
        table->setUpdatesEnabled(true);
    };
    auto timer=new QTimer(dialog);connect(timer,&QTimer::timeout,dialog,refresh);timer->start(2000);refresh();
    auto buttons=new QDialogButtonBox(QDialogButtonBox::Close,dialog);layout->addWidget(buttons);
    auto import=buttons->addButton(NetLinkUi::text("Import selected computer"),QDialogButtonBox::ActionRole);
    connect(import,&QPushButton::clicked,dialog,[this,dialog,table,records,import] {
        const int row=table->currentRow();if(row<0 || row>=records->size()) return;
        const auto record=(*records)[row];
        if(record.port!=VeyonCore::config().veyonServerPort()) {notify(dialog,QMessageBox::Warning,"Discovery",NetLinkUi::text("Port differs from the configured port: add the computer manually."));return;}
        auto confirmation=new QMessageBox(QMessageBox::Question,NetLinkUi::text("Import computer"),record.name+" — "+record.address+NetLinkUi::text("\nConfirm that this computer belongs to the school?"),QMessageBox::Yes|QMessageBox::No,dialog);
        confirmation->setTextFormat(Qt::PlainText);confirmation->setAttribute(Qt::WA_DeleteOnClose);import->setEnabled(false);
        connect(confirmation,&QMessageBox::finished,dialog,[dialog,record,import](int answer) {
        import->setEnabled(true);if(answer!=QMessageBox::Yes) return;
        auto& config=VeyonCore::config();
        if(!Configuration::LocalStore(Configuration::LocalStore::System).isWritable()) {
            notify(dialog,QMessageBox::Warning,"Discovery",NetLinkUi::text("Administrator rights are required to import a computer into configuration."));return;
        }
        auto objects=config.value("NetworkObjects","BuiltinDirectory",QJsonArray{}).toJsonArray();
        QUuid location;
        for(const auto& value:objects) {const NetworkObject object(value.toObject());if(object.type()==NetworkObject::Type::Host && object.hostAddress()==record.address) {notify(dialog,QMessageBox::Information,"Discovery",NetLinkUi::text("Computer already configured."));return;}if(object.type()==NetworkObject::Type::Location && location.isNull()) location=object.uid();}
        if(location.isNull()) {location=QUuid::createUuid();objects.append(NetworkObject(NetworkObject::Type::Location,"Discovery",{},{},{},location).toJson());}
        objects.append(NetworkObject(NetworkObject::Type::Host,record.name.left(120),record.address,{},{},QUuid::createUuid(),location).toJson());
        config.setValue("NetworkObjects",objects,"BuiltinDirectory");
        ConfigurationManager manager;
        if(!manager.saveConfiguration()) {notify(dialog,QMessageBox::Warning,"Discovery",manager.errorString());return;}
        notify(dialog,QMessageBox::Information,"Discovery",NetLinkUi::text("Computer imported. Restart the Master to display it; key authentication is still required."));
        });confirmation->open();
    });
    connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::reject);dialog->show();
}
