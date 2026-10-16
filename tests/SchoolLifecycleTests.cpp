// SPDX-License-Identifier: GPL-2.0-or-later
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include <QtTest>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QLineEdit>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QProcess>
#include <QTcpServer>
#include <QTableWidget>
#include "SchoolFeatures.h"
#include "NetLinkUi.h"
#include "SchoolPolicyResolver.h"
#include "VeyonMasterInterface.h"
#include "VeyonWorkerInterface.h"
#include "../worker/src/FeatureWorkerManagerConnection.h"

class IsolatedSchoolMaster : public VeyonMasterInterface {
public:
    IsolatedSchoolMaster() : VeyonMasterInterface(nullptr), local(Computer({},"local",{},{},{})) {}
    QWidget* mainWindow() override { return &window; }
    Configuration::Object* userConfigurationObject() override { return nullptr; }
    void reloadSubFeatures() override {}
    ComputerControlInterface& localSessionControlInterface() override { return local; }
    const ComputerControlInterfaceList& allComputerControlInterfaces() const override { return clients; }
    ComputerControlInterfaceList selectedComputerControlInterfaces() const override { return clients; }
    ComputerControlInterfaceList filteredComputerControlInterfaces() const override { return clients; }
    QWidget window;
    ComputerControlInterface local;
    ComputerControlInterfaceList clients;
};
class IsolatedSchoolWorker : public QObject, public VeyonWorkerInterface {
public:
    explicit IsolatedSchoolWorker(int& count) : replies(count) {}
    bool sendFeatureMessageReply(const FeatureMessage&) override { ++replies; return true; }
    void finishAfterReplies() override {++finishes;}
    int finishes=0;
    int& replies;
};

// Every runtime object uses a private friend-only constructor: no production
// configuration, real keys, service, network listener or remote PC is touched.
class SchoolLifecycleTests : public QObject {
    Q_OBJECT
public:
    static int attendanceChild(quint16 port,bool accept) {
        class Worker : public QObject,public VeyonWorkerInterface {
        public:
            FeatureWorkerManagerConnection* connection=nullptr;
            bool sendFeatureMessageReply(const FeatureMessage& message) override {return connection->sendMessage(message);}
            void finishAfterReplies() override {connection->finishAfterReplies();}
        } worker;
        SchoolFeatures features(nullptr,false);
        worker.connection=new FeatureWorkerManagerConnection(worker,features.m_attendanceUid,port,&worker);
        QTimer::singleShot(100,&worker,[&] {
            if(!features.handleFeatureMessage(worker,FeatureMessage(features.m_attendanceUid,SchoolFeatures::AskName).addArgument(0,QUuid::createUuid().toString(QUuid::WithoutBraces)))) {qApp->exit(2);return;}
            QTimer::singleShot(50,&worker,[&] {features.m_studentDialog->findChild<QLineEdit*>()->setText("Anna Rossi");if(accept) features.m_studentDialog->accept();else features.m_studentDialog->reject();});
        });
        QTimer::singleShot(4000,qApp,[]{qApp->exit(3);});
        return qApp->exec();
    }
private:
    using Callback = std::function<void(const QHostInfo&)>;
    struct FakeDns {
        QHash<int,Callback> callbacks;
        QList<int> aborted;
        int next=1;
        SchoolPolicyResolver::Lookup lookup() {return [this](const QString&,QObject*,const Callback& callback) {const int id=next++;callbacks.insert(id,callback);return id;};}
        SchoolPolicyResolver::Abort abort() {return [this](int id) {aborted.append(id);};}
        void reply(int id,const QList<QHostAddress>& addresses,QHostInfo::HostInfoError error=QHostInfo::NoError) {
            QHostInfo info(id);info.setAddresses(addresses);info.setError(error);callbacks.value(id)(info);
        }
    };
    static ComputerControlInterface::Pointer client(const QString& host="192.0.2.25") {
        return ComputerControlInterface::Pointer::create(Computer({},"PC selezionato",host,{},{}));
    }
    static SchoolDiscovery::Message discoveryPacket(const QString& action,const QString& endpoint,quint64 sequence=1,const QString& id={}) {
        SchoolDiscovery::Message message;message.action=action;message.id=id.isEmpty()?"urn:uuid:"+QUuid::createUuid().toString(QUuid::WithoutBraces):id;message.instance=123;message.sequence=sequence;
        SchoolDiscovery::Record record;record.endpoint=endpoint;record.name="PC di prova";record.address="198.51.100.99";record.port=11100;record.client=true;message.records.append(record);return message;
    }
private Q_SLOTS:
    void selectedLanguageChangesLabelsNotFeatureIdentifiers() {
        struct Restore {QLocale value;~Restore(){QLocale::setDefault(value);}} restore{QLocale()};
        QLocale::setDefault(QLocale("en_US"));SchoolFeatures features(nullptr,false);
        QCOMPARE(features.featureList()[1].name(),QString("Appello"));
        QCOMPARE(features.featureList()[1].displayName(),QString("Attendance"));
        QCOMPARE(features.featureList()[2].name(),QString("Discovery"));
        QCOMPARE(features.featureList()[2].displayName(),QString("Find clients"));
    }
    void attendanceLastWindowDeliversReplyBeforeWorkerExits_data() {QTest::addColumn<bool>("accept");QTest::newRow("confirmed")<<true;QTest::newRow("cancelled")<<false;}
    void attendanceLastWindowDeliversReplyBeforeWorkerExits() {
        QFETCH(bool,accept);QTcpServer server;QVERIFY(server.listen(QHostAddress::LocalHost,0));QList<FeatureMessage> messages;
        connect(&server,&QTcpServer::newConnection,this,[&] {auto socket=server.nextPendingConnection();connect(socket,&QTcpSocket::readyRead,&server,[&,socket] {FeatureMessage message;while(message.isReadyForReceive(socket)) {if(!message.receive(socket)) break;messages.append(message);}});});
        QProcess child;child.start(QCoreApplication::applicationFilePath(),{"--attendance-child",QString::number(server.serverPort()),accept?"accept":"cancel"});
        QTRY_COMPARE_WITH_TIMEOUT(child.state(),QProcess::NotRunning,6500);QCOMPARE(child.exitStatus(),QProcess::NormalExit);QCOMPARE(child.exitCode(),0);
        QTRY_VERIFY_WITH_TIMEOUT(messages.size()>=2,1000);QCOMPARE(messages.last().command(),int(SchoolFeatures::NameReply));QCOMPARE(messages.last().argument(1).toString(),accept?QString("Anna Rossi"):QString{});
        const auto ci=client();ci->setProperty("schoolAttendanceRequest",messages.last().argument(0));SchoolFeatures features(nullptr,false);QVERIFY(features.handleFeatureMessage(ci,messages.last()));QCOMPARE(ci->studentName(),accept?QString("Anna Rossi"):QString{});
    }
    void dialogCyclesReturnImmediatelyAndRemainIdempotent() {
        IsolatedSchoolMaster master;
        const auto ci=client();master.clients={ci};
        SchoolFeatures features(nullptr,false);
        // The real Master has already painted its window before an action can
        // be clicked. Mirror that state: Windows/offscreen font enumeration on
        // the first-ever widget must not be measured as a feature event loop.
        QFontDatabase().families();master.window.show();QTest::qWait(1);
        qint64 slowest=0;
        int pulses=0;QTimer timer;connect(&timer,&QTimer::timeout,this,[&pulses] {++pulses;});timer.start(1);
        for(int cycle=0;cycle<25;++cycle) for(const auto& feature:features.featureList()) {
            QElapsedTimer elapsed;elapsed.start();
            QVERIFY(features.startFeature(master,feature,master.clients));
            const auto duration=elapsed.elapsed();slowest=qMax(slowest,duration);
            QVERIFY2(duration<500,qPrintable(QString("Feature dialog did not return within 500 ms: %1 ms").arg(duration)));
            QPointer<QDialog> dialog=feature.uid()==features.m_guardUid ? features.m_guardDialog : feature.uid()==features.m_attendanceUid ? features.m_attendanceDialog : features.m_discoveryDialog;
            QVERIFY(dialog);QVERIFY(dialog->isVisible());
            QVERIFY(features.startFeature(master,feature,{}));
            QCOMPARE(dialog,feature.uid()==features.m_guardUid ? features.m_guardDialog : feature.uid()==features.m_attendanceUid ? features.m_attendanceDialog : features.m_discoveryDialog);
            QTest::qWait(2);
            QVERIFY(dialog->windowTitle().endsWith(NetLinkUi::copyrightText()));
            QVERIFY(dialog->findChildren<QLabel*>("netlinkCopyright").isEmpty());
            dialog->reject();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QVERIFY(dialog.isNull());
        }
        qInfo("NetLink School dialog open maximum after console initialization: %lld ms",static_cast<long long>(slowest));
        QVERIFY(pulses>0);QVERIFY(features.m_guardTargets.isEmpty());QVERIFY(!features.m_resolution);
    }
    void selectedClientSnapshotAndEventDrivenStatus() {
        IsolatedSchoolMaster master;
        const auto original=client();const auto replacement=client("192.0.2.26");
        SchoolFeatures features(nullptr,false);
        QVERIFY(features.startFeature(master,features.featureList().first(),{original}));
        auto table=features.m_guardDialog->findChild<QTableWidget*>();QVERIFY(table);QCOMPARE(table->rowCount(),1);
        auto status=table->item(0,1);original->setNetworkPolicyStatus("confermato");QCOMPARE(status->text(),QString("confermato"));
        QVERIFY(features.startFeature(master,features.featureList().first(),{replacement}));
        QCOMPARE(table->item(0,1),status);QCOMPARE(table->item(0,0)->text(),original->computer().name());
        replacement->setNetworkPolicyStatus("altro PC");QCOMPARE(status->text(),QString("confermato"));
    }
    void featureDestructionClosesAllOwnedDialogs() {
        IsolatedSchoolMaster master;
        auto features=new SchoolFeatures(nullptr,false);
        for(const auto& feature:features->featureList()) QVERIFY(features->startFeature(master,feature,{}));
        QPointer<QDialog> guard=features->m_guardDialog,attendance=features->m_attendanceDialog,discovery=features->m_discoveryDialog;
        delete features;QVERIFY(!guard);QVERIFY(!attendance);QVERIFY(!discovery);
    }
    void closingInternetGuardMonitorNeverReleasesPolicy() {
        IsolatedSchoolMaster master;const auto ci=client();SchoolFeatures features(nullptr,false);
        features.m_guardTargets.append({ci.data(),{"192.0.2.25"},features.m_clock.elapsed()});
        QVERIFY(features.startFeature(master,features.featureList().first(),{ci}));features.m_guardDialog->reject();
        QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QCOMPARE(features.m_guardTargets.size(),1);QCOMPARE(features.m_guardTargets.first().control.data(),ci.data());
    }
    void resolverRequiresEveryHostAndRejectsNegativeDns() {
        FakeDns dns;SchoolPolicyResolver resolver({"one.invalid","two.invalid"},{"192.0.2.99"},nullptr,dns.lookup(),dns.abort(),500);
        QSignalSpy finished(&resolver,&SchoolPolicyResolver::finished);resolver.start();
        dns.reply(1,{QHostAddress("192.0.2.25")});QCOMPARE(finished.count(),0);
        dns.reply(2,{},QHostInfo::HostNotFound);QCOMPARE(finished.count(),1);QVERIFY(!finished.first()[0].toBool());QVERIFY(finished.first()[1].toStringList().isEmpty());
        dns.reply(2,{QHostAddress("192.0.2.26")});QCOMPARE(finished.count(),1);
    }
    void resolverIgnoresLateReplyAfterCancellation() {
        FakeDns dns;SchoolPolicyResolver resolver({"one.invalid"},{},nullptr,dns.lookup(),dns.abort(),100);
        QSignalSpy finished(&resolver,&SchoolPolicyResolver::finished);resolver.start();resolver.cancel();resolver.cancel();
        QCOMPARE(dns.aborted,QList<int>({1}));dns.reply(1,{QHostAddress("192.0.2.25")});QCOMPARE(finished.count(),0);
    }
    void resolverTimeoutIsBoundedAndLateSuccessCannotApply() {
        FakeDns dns;SchoolPolicyResolver resolver({"one.invalid"},{},nullptr,dns.lookup(),dns.abort(),20);
        QSignalSpy finished(&resolver,&SchoolPolicyResolver::finished);resolver.start();
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(),1,1000);QVERIFY(!finished.first()[0].toBool());QCOMPARE(dns.aborted,QList<int>({1}));
        dns.reply(1,{QHostAddress("192.0.2.25")});QCOMPARE(finished.count(),1);
    }
    void resolverDeduplicatesAndEmitsOnce() {
        FakeDns dns;SchoolPolicyResolver resolver({"192.0.2.25","one.invalid","one.invalid"},{"192.0.2.25"},nullptr,dns.lookup(),dns.abort(),500);
        QSignalSpy finished(&resolver,&SchoolPolicyResolver::finished);resolver.start();resolver.start();QCOMPARE(dns.callbacks.size(),1);
        dns.reply(1,{QHostAddress("192.0.2.25"),QHostAddress("192.0.2.26")});QCOMPARE(finished.count(),1);QVERIFY(finished.first()[0].toBool());
        QCOMPARE(finished.first()[1].toStringList(),QStringList({"192.0.2.25","192.0.2.26"}));
    }
    void resolverRejectsUnusableAddressesDespiteValidResources() {
        FakeDns dns;SchoolPolicyResolver resolver({"one.invalid"},{"192.0.2.99"},nullptr,dns.lookup(),dns.abort(),500);
        QSignalSpy finished(&resolver,&SchoolPolicyResolver::finished);resolver.start();dns.reply(1,{QHostAddress::LocalHost});
        QCOMPARE(finished.count(),1);QVERIFY(!finished.first()[0].toBool());
    }
    void releaseCancelsOnlyAnOverlappingBatchAndRepeatedBlockDoesNotInvalidate() {
        const auto selected=client(),other=client("192.0.2.26");FakeDns dns;
        SchoolFeatures features(nullptr,false);
        auto resolver=new SchoolPolicyResolver({"one.invalid"},{},&features,dns.lookup(),dns.abort(),500);
        features.m_resolution=resolver;features.m_resolutionClients={selected};resolver->start();
        features.policy({selected},true);QCOMPARE(features.m_resolution.data(),resolver);QVERIFY(dns.aborted.isEmpty());
        features.policy({other},false);QCOMPARE(features.m_resolution.data(),resolver);QVERIFY(dns.aborted.isEmpty());
        features.policy({selected},false);QVERIFY(!features.m_resolution);QCOMPARE(dns.aborted,QList<int>({1}));QVERIFY(features.m_resolutionClients.isEmpty());
        dns.reply(1,{QHostAddress("192.0.2.25")});QVERIFY(features.m_guardTargets.isEmpty());
    }
    void disconnectedClientIsNeverAddedToLeaseRenewals() {
        SchoolFeatures features(nullptr,false);const auto ci=client();features.beginPolicyResolution({ci},{});
        QVERIFY(features.m_guardTargets.isEmpty());QVERIFY(!features.m_resolution);QVERIFY(ci->networkPolicyStatus().contains("non connesso"));
    }
    void studentDialogDoesNotOutliveWorkerOrReplyToDeadWorker() {
        SchoolFeatures features(nullptr,false);int replies=0;auto worker=new IsolatedSchoolWorker(replies);
        QVERIFY(features.handleFeatureMessage(*worker,FeatureMessage(features.m_attendanceUid,SchoolFeatures::AskName).addArgument(0,QUuid::createUuid().toString(QUuid::WithoutBraces))));
        QPointer<QDialog> dialog=features.m_studentDialog;QVERIFY(dialog);delete worker;
        QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QVERIFY(!dialog);QCOMPARE(replies,0);
    }
    void studentDialogRejectsInvalidRequestAndRepliesOnce() {
        SchoolFeatures features(nullptr,false);int replies=0;IsolatedSchoolWorker worker(replies);
        QVERIFY(features.handleFeatureMessage(worker,FeatureMessage(features.m_attendanceUid,SchoolFeatures::AskName).addArgument(0,"invalid")));QVERIFY(!features.m_studentDialog);
        const auto id=QUuid::createUuid().toString(QUuid::WithoutBraces);
        QVERIFY(features.handleFeatureMessage(worker,FeatureMessage(features.m_attendanceUid,SchoolFeatures::AskName).addArgument(0,id)));
        const auto dialog=features.m_studentDialog;QVERIFY(!dialog->testAttribute(Qt::WA_QuitOnClose));dialog->findChild<QLineEdit*>()->setText("Anna Rossi");
        QVERIFY(features.handleFeatureMessage(worker,FeatureMessage(features.m_attendanceUid,SchoolFeatures::AskName).addArgument(0,id)));QCOMPARE(features.m_studentDialog,dialog);QCOMPARE(dialog->findChild<QLineEdit*>()->text(),QString("Anna Rossi"));
        dialog->accept();QCOMPARE(replies,1);QCOMPARE(worker.finishes,1);
        QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QVERIFY(!features.m_studentDialog);
    }
    void attendanceRejectsEmptyAndReplayedReplies() {
        SchoolFeatures features(nullptr,false);const auto ci=client();
        QVERIFY(features.handleFeatureMessage(ci,FeatureMessage(features.m_attendanceUid,SchoolFeatures::NameReply).addArgument(0,QString{}).addArgument(1,"non autorizzato")));QVERIFY(ci->studentName().isEmpty());
        const auto id=QUuid::createUuid().toString(QUuid::WithoutBraces);ci->setProperty("schoolAttendanceRequest",id);
        QVERIFY(features.handleFeatureMessage(ci,FeatureMessage(features.m_attendanceUid,SchoolFeatures::NameReply).addArgument(0,id).addArgument(1,"Anna Rossi")));QCOMPARE(ci->studentName(),QString("Anna Rossi"));
        QVERIFY(features.handleFeatureMessage(ci,FeatureMessage(features.m_attendanceUid,SchoolFeatures::NameReply).addArgument(0,id).addArgument(1,"replay")));QCOMPARE(ci->studentName(),QString("Anna Rossi"));
    }
    void discoveryRepeatedProbeIsCoalescedWithoutOpeningSockets() {
        SchoolDiscovery::Agent agent(true,{},11100,nullptr,false);
        for(int i=0;i<1000;++i) {agent.probe();}
        QCOMPARE(agent.m_queries.size(),1);
        SchoolDiscovery::Agent clientAgent(false,{},11100,nullptr,false);clientAgent.probe();QVERIFY(clientAgent.m_queries.isEmpty());
    }
    void discoveryHidesLocalWifiVirtualAndLoopbackAddresses() {
        SchoolDiscovery::Agent agent(true,{},11100,nullptr,false);
        for(const auto& local:{"192.168.1.195","172.24.176.1"}) agent.m_localIpv4.insert(QHostAddress(local).toIPv4Address());
        for(const auto& source:{"192.168.1.195","172.24.176.1","::ffff:172.24.176.1","127.0.0.1","127.2.3.4","::ffff:127.0.0.1"}) {
            const auto endpoint="urn:uuid:"+QUuid::createUuid().toString(QUuid::WithoutBraces);
            agent.process(SchoolDiscovery::encode(discoveryPacket("Hello",endpoint)),QHostAddress(source),false);
            QVERIFY(!agent.m_records.contains(endpoint));QVERIFY(agent.records().isEmpty());
        }
        agent.m_localIpv4.clear();QVERIFY(agent.records().isEmpty());
    }
    void discoverySelfFilterKeepsRemoteSameNameClonedIdentityAndOtherSubnets() {
        SchoolDiscovery::Agent agent(true,{},11100,nullptr,false),localClient(false,{},11100,nullptr,false);
        agent.m_localIpv4.insert(QHostAddress("192.168.1.195").toIPv4Address());
        agent.m_localIpv4.insert(QHostAddress("172.24.176.1").toIPv4Address());
        const auto sources=QStringList{"192.168.1.109","172.24.176.20","10.20.30.40"};
        for(int i=0;i<sources.size();++i) {
            // A matching hostname or machine-derived endpoint alone must never
            // hide a remote computer (e.g. a cloned classroom image).
            const auto endpoint=i==0?localClient.m_endpoint:"urn:uuid:"+QUuid::createUuid().toString(QUuid::WithoutBraces);
            auto packet=discoveryPacket("Hello",endpoint);packet.records.first().name=QHostInfo::localHostName();packet.records.first().address="192.168.1.195";
            agent.process(SchoolDiscovery::encode(packet),QHostAddress(sources[i]),true);
        }
        QCOMPARE(agent.records().size(),sources.size());
        QSet<QString> visible;for(const auto& record:agent.records()) visible.insert(record.address);
        for(const auto& source:sources) QVERIFY(visible.contains(source));
        // Injecting cached IPs exercises the defensive view filter separately
        // from refreshInterfaces(), which also prunes local observations.
        agent.m_localIpv4.insert(QHostAddress(sources.first()).toIPv4Address());QCOMPARE(agent.records().size(),2);
        agent.m_localIpv4.remove(QHostAddress(sources.first()).toIPv4Address());QCOMPARE(agent.records().size(),3);
    }
    void discoveryIpv6CannotPoisonOrReplaceIpv4Announcements() {
        SchoolDiscovery::Agent agent(true,{},11100,nullptr,false);const auto endpoint="urn:uuid:"+QUuid::createUuid().toString(QUuid::WithoutBraces);
        const auto first=discoveryPacket("Hello",endpoint);
        for(const auto& source:{"2001:db8::25","fe80::25","::1","::192.0.2.25"}) {agent.process(SchoolDiscovery::encode(first),QHostAddress(source),false);QVERIFY(agent.records().isEmpty());QVERIFY(!agent.m_seen.contains(first.id));}
        agent.process(SchoolDiscovery::encode(first),QHostAddress("192.0.2.25"),false);QCOMPARE(agent.records().size(),1);QCOMPARE(agent.records().first().address,QString("192.0.2.25"));
        agent.process(SchoolDiscovery::encode(discoveryPacket("Hello",endpoint,9)),QHostAddress("2001:db8::25"),false);QCOMPARE(agent.records().first().address,QString("192.0.2.25"));
        const auto bye=discoveryPacket("Bye",endpoint,10);agent.process(SchoolDiscovery::encode(bye),QHostAddress("2001:db8::25"),false);QCOMPARE(agent.records().size(),1);QVERIFY(!agent.m_seen.contains(bye.id));
        agent.process(SchoolDiscovery::encode(bye),QHostAddress("192.0.2.25"),false);QVERIFY(agent.records().isEmpty());
    }
    void discoveryIpv4SourcesRemainValidAcrossSubnetsAndIgnoreClaimedAddresses() {
        SchoolDiscovery::Agent agent(true,{},11100,nullptr,false);
        for(const auto& source:{"10.20.30.40","172.24.176.20","192.0.2.25"}) {
            const auto endpoint="urn:uuid:"+QUuid::createUuid().toString(QUuid::WithoutBraces);const auto packet=discoveryPacket("Hello",endpoint);
            agent.process(SchoolDiscovery::encode(packet),QHostAddress(source),true);QVERIFY(agent.m_records.contains(endpoint));QCOMPARE(agent.m_records[endpoint].address,QString(source));
        }
        QCOMPARE(agent.records().size(),3);
    }
    void discoveryNormalizesMappedIpv4AndRejectsUnspecifiedOrMulticastSources() {
        SchoolDiscovery::Agent agent(true,{},11100,nullptr,false);const auto endpoint="urn:uuid:"+QUuid::createUuid().toString(QUuid::WithoutBraces);
        const auto packet=discoveryPacket("Hello",endpoint);agent.process(SchoolDiscovery::encode(packet),QHostAddress("::ffff:192.0.2.25"),false);
        QCOMPARE(agent.records().size(),1);QCOMPARE(agent.records().first().address,QString("192.0.2.25"));
        for(const auto& source:{"0.0.0.0","::","224.0.0.1","::ffff:224.0.0.1","255.255.255.255"}) {
            const auto rejected=discoveryPacket("Hello","urn:uuid:"+QUuid::createUuid().toString(QUuid::WithoutBraces));agent.process(SchoolDiscovery::encode(rejected),QHostAddress(source),false);QCOMPARE(agent.records().size(),1);
        }
    }
    void discoveryStillAnswersIpv6ProbesButNeverDisplaysIpv6Records() {
        SchoolDiscovery::Agent clientAgent(false,{},11100,nullptr,false);SchoolDiscovery::Message probe;probe.action="Probe";
        const auto answer=clientAgent.process(SchoolDiscovery::encode(probe),QHostAddress("2001:db8::25"),false);SchoolDiscovery::Message decoded;QVERIFY(SchoolDiscovery::decode(answer,decoded));QCOMPARE(decoded.action.section('/',-1),QString("ProbeMatches"));
        SchoolDiscovery::Agent masterAgent(true,{},11100,nullptr,false);SchoolDiscovery::Record record;record.endpoint="urn:uuid:"+QUuid::createUuid().toString(QUuid::WithoutBraces);record.address="2001:db8::25";masterAgent.m_records.insert(record.endpoint,record);QVERIFY(masterAgent.records().isEmpty());
        record.address="::ffff:192.0.2.25";masterAgent.m_records.insert(record.endpoint,record);QCOMPARE(masterAgent.records().first().address,QString("192.0.2.25"));
    }
    void discoveryDualStackProbeDuplicatesReceiveOneReplyPerFamily() {
        for(const auto& action:{"Probe","Resolve"}) for(bool ipv6First:{true,false}) {
            SchoolDiscovery::Agent agent(false,{},11100,nullptr,false);SchoolDiscovery::Message query;query.action=action;query.id="urn:uuid:"+QUuid::createUuid().toString(QUuid::WithoutBraces);query.resolve=agent.m_endpoint;
            const auto bytes=SchoolDiscovery::encode(query);
            const auto first=QHostAddress(ipv6First?"2001:db8::25":"192.0.2.25"),second=QHostAddress(ipv6First?"192.0.2.25":"2001:db8::25");
            for(const auto& source:{first,second}) {
                SchoolDiscovery::Message answer;QVERIFY(SchoolDiscovery::decode(agent.process(bytes,source,false),answer));
                QCOMPARE(answer.action.section('/',-1),QString(action)+"Matches");QCOMPARE(answer.relatesTo,query.id);
                QCOMPARE(answer.records.size(),1);QCOMPARE(answer.records.first().endpoint,agent.m_endpoint);QVERIFY(answer.records.first().client);
            }
            QVERIFY(agent.process(bytes,QHostAddress("2001:db8::25"),false).isEmpty());
            QVERIFY(agent.process(bytes,QHostAddress("192.0.2.25"),false).isEmpty());
        }
    }
    void discoveryResolveOnlyAnswersRequestedEndpoint() {
        for(bool master:{false,true}) {
            SchoolDiscovery::Agent agent(master,{},11100,nullptr,false);SchoolDiscovery::Message query;query.action="Resolve";
            query.resolve="urn:uuid:"+QUuid::createUuid().toString(QUuid::WithoutBraces);
            QVERIFY(agent.process(SchoolDiscovery::encode(query),QHostAddress("192.0.2.25"),false).isEmpty());
            query.resolve=agent.m_endpoint;SchoolDiscovery::Message answer;
            QVERIFY(SchoolDiscovery::decode(agent.process(SchoolDiscovery::encode(query),QHostAddress("192.0.2.25"),false),answer));
            QCOMPARE(answer.action.section('/',-1),QString("ResolveMatches"));QCOMPARE(answer.records.size(),1);
            QCOMPARE(answer.records.first().endpoint,agent.m_endpoint);QCOMPARE(answer.records.first().proxy,master);
        }
    }
    void discoveryMatchedRepliesStayIpv4AndIgnoreNewerIpv6Instances() {
        for(const auto& action:{"ProbeMatches","ResolveMatches"}) {
            SchoolDiscovery::Agent agent(true,{},11100,nullptr,false);const auto endpoint="urn:uuid:"+QUuid::createUuid().toString(QUuid::WithoutBraces);
            auto reply=discoveryPacket(action,endpoint);reply.relatesTo="urn:uuid:"+QUuid::createUuid().toString(QUuid::WithoutBraces);agent.m_queries.insert(reply.relatesTo,0);
            agent.process(SchoolDiscovery::encode(reply),QHostAddress("2001:db8::25"),false);QVERIFY(agent.records().isEmpty());
            agent.process(SchoolDiscovery::encode(reply),QHostAddress("172.24.176.20"),false);QCOMPARE(agent.records().size(),1);QCOMPARE(agent.records().first().address,QString("172.24.176.20"));
            auto newer=discoveryPacket("Hello",endpoint,2);newer.instance=reply.instance+1;agent.process(SchoolDiscovery::encode(newer),QHostAddress("2001:db8::25"),false);
            auto ipv4=discoveryPacket("Hello",endpoint,3);agent.process(SchoolDiscovery::encode(ipv4),QHostAddress("172.24.176.21"),false);QCOMPARE(agent.records().first().address,QString("172.24.176.21"));
        }
    }
    void discoveryTableRetainsSelectionAndClosesWithPendingTimers() {
        IsolatedSchoolMaster master;SchoolFeatures features(nullptr,false);
        features.m_discovery=new SchoolDiscovery::Agent(true,{},11100,&features,false);
        SchoolDiscovery::Record first;first.endpoint="urn:uuid:00000000-0000-0000-0000-000000000001";first.name="<b>Nome non attendibile</b>";first.address="192.0.2.25";
        SchoolDiscovery::Record second=first;second.endpoint="urn:uuid:00000000-0000-0000-0000-000000000002";second.address="192.0.2.26";
        features.m_discovery->m_records.insert(first.endpoint,first);features.m_discovery->m_records.insert(second.endpoint,second);
        auto ipv6=first;ipv6.endpoint="urn:uuid:00000000-0000-0000-0000-000000000003";ipv6.address="2001:db8::25";features.m_discovery->m_records.insert(ipv6.endpoint,ipv6);
        features.m_discovery->m_localIpv4.insert(QHostAddress("192.168.1.195").toIPv4Address());
        auto self=first;self.endpoint="urn:uuid:00000000-0000-0000-0000-000000000004";self.address="192.168.1.195";features.m_discovery->m_records.insert(self.endpoint,self);
        features.discovery(master);QPointer<QDialog> dialog=features.m_discoveryDialog;QVERIFY(dialog);
        auto table=dialog->findChild<QTableWidget*>();QVERIFY(table);QCOMPARE(table->rowCount(),2);QCOMPARE(table->horizontalHeaderItem(1)->text(),QString("IPv4 rilevato"));table->selectRow(1);
        auto original=table->item(0,0);auto timer=dialog->findChild<QTimer*>();QVERIFY(timer);timer->start(10);QTest::qWait(25);QCOMPARE(table->item(0,0),original);
        first.name="PC aggiornato";features.m_discovery->m_records.insert(first.endpoint,first);
        QTRY_COMPARE_WITH_TIMEOUT(table->item(0,0)->text(),QString("PC aggiornato"),1000);QCOMPARE(table->currentRow(),1);QCOMPARE(table->item(1,1)->text(),second.address);
        features.m_discovery->m_localIpv4.insert(QHostAddress(second.address).toIPv4Address());
        QTRY_COMPARE_WITH_TIMEOUT(table->rowCount(),1,1000);QCOMPARE(table->currentRow(),-1);QVERIFY(table->selectedItems().isEmpty());
        features.m_discovery->m_localIpv4.remove(QHostAddress(second.address).toIPv4Address());
        QTRY_COMPARE_WITH_TIMEOUT(table->rowCount(),2,1000);QCOMPARE(table->currentRow(),-1);
        dialog->reject();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QVERIFY(!dialog);QTest::qWait(20);
    }
};
int main(int argc,char** argv) {
    QApplication app(argc,argv);const auto arguments=app.arguments();
    if(arguments.size()==4 && arguments[1]=="--attendance-child") return SchoolLifecycleTests::attendanceChild(arguments[2].toUShort(),arguments[3]=="accept");
    SchoolLifecycleTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "SchoolLifecycleTests.moc"
