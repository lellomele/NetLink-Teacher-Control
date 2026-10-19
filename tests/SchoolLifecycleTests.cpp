// SPDX-License-Identifier: GPL-2.0-or-later
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include <QtTest>
#include <QCheckBox>
#include <QComboBox>
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
#include "SchoolDiscoveryKnownHosts.h"
#include "VeyonMasterInterface.h"
#include "VeyonWorkerInterface.h"
#include "../worker/src/FeatureWorkerManagerConnection.h"

class IsolatedSchoolMaster : public VeyonMasterInterface {
public:
    IsolatedSchoolMaster() : VeyonMasterInterface(nullptr), local(Computer({},"local",{},{},{})) {}
    QWidget* mainWindow() override { return &window; }
    Configuration::Object* userConfigurationObject() override { return &preferences; }
    void reloadSubFeatures() override {}
    ComputerControlInterface& localSessionControlInterface() override { return local; }
    const ComputerControlInterfaceList& allComputerControlInterfaces() const override { return clients; }
    ComputerControlInterfaceList selectedComputerControlInterfaces() const override { return clients; }
    ComputerControlInterfaceList filteredComputerControlInterfaces() const override { return clients; }
    QWidget window;
    ComputerControlInterface local;
    ComputerControlInterfaceList clients;
    Configuration::Object preferences;
};
class IsolatedSchoolWorker : public QObject, public VeyonWorkerInterface {
public:
    explicit IsolatedSchoolWorker(int& count) : replies(count) {}
    bool sendFeatureMessageReply(const FeatureMessage& message) override { lastReply=message; ++replies; return true; }
    void finishAfterReplies() override {++finishes;}
    int finishes=0;
    int& replies;
    FeatureMessage lastReply;
};

// Every runtime object uses a private friend-only constructor: no production
// configuration, real keys, service, network listener or remote PC is touched.
class SchoolLifecycleTests : public QObject {
    Q_OBJECT
public:
    static int attendanceChild(quint16 port,const QString& action) {
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
            QTimer::singleShot(50,&worker,[&] {
                auto name=features.m_studentDialog->findChild<QLineEdit*>();name->setText("Anna Rossi");
                auto buttons=features.m_studentDialog->findChild<QDialogButtonBox*>();
                if(action=="return" || action=="enter") {
                    buttons->button(QDialogButtonBox::Cancel)->setDefault(true);name->setFocus();
                    QTest::keyClick(name,action=="return"?Qt::Key_Return:Qt::Key_Enter);
                } else buttons->button(action=="accept"?QDialogButtonBox::Ok:QDialogButtonBox::Cancel)->click();
            });
        });
        QTimer::singleShot(4000,qApp,[]{qApp->exit(3);});
        return qApp->exec();
    }
private:
    using Callback = std::function<void(const QHostInfo&)>;
    struct FakeDns {
        QHash<int,Callback> callbacks;
        QHash<int,QString> requested;
        QList<int> aborted;
        int next=1;
        SchoolPolicyResolver::Lookup lookup() {return [this](const QString& host,QObject*,const Callback& callback) {const int id=next++;callbacks.insert(id,callback);requested.insert(id,host);return id;};}
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
    void attendanceLastWindowDeliversReplyBeforeWorkerExits_data() {
        QTest::addColumn<QString>("action");
        for(const auto& action:QStringList{"accept","cancel","return","enter"}) QTest::newRow(qPrintable(action))<<action;
    }
    void attendanceLastWindowDeliversReplyBeforeWorkerExits() {
        QFETCH(QString,action);QTcpServer server;QVERIFY(server.listen(QHostAddress::LocalHost,0));QList<FeatureMessage> messages;
        connect(&server,&QTcpServer::newConnection,this,[&] {auto socket=server.nextPendingConnection();connect(socket,&QTcpSocket::readyRead,&server,[&,socket] {FeatureMessage message;while(message.isReadyForReceive(socket)) {if(!message.receive(socket)) break;messages.append(message);}});});
        QProcess child;child.start(QCoreApplication::applicationFilePath(),{"--attendance-child",QString::number(server.serverPort()),action});
        QTRY_COMPARE_WITH_TIMEOUT(child.state(),QProcess::NotRunning,6500);QCOMPARE(child.exitStatus(),QProcess::NormalExit);QCOMPARE(child.exitCode(),0);
        QTRY_VERIFY_WITH_TIMEOUT(messages.size()>=2,1000);QCOMPARE(messages.size(),2);QCOMPARE(messages.last().command(),int(SchoolFeatures::NameReply));QCOMPARE(messages.last().argument(1).toString(),action!="cancel"?QString("Anna Rossi"):QString{});
        const auto ci=client();ci->setProperty("schoolAttendanceRequest",messages.last().argument(0));SchoolFeatures features(nullptr,false);QVERIFY(features.handleFeatureMessage(ci,messages.last()));QCOMPARE(ci->studentName(),action!="cancel"?QString("Anna Rossi"):QString{});
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
    void disconnectedClientIsNeverAddedToLeaseRenewals_data() {
        QTest::addColumn<QString>("locale");QTest::addColumn<QString>("expectedStatus");
        QTest::newRow("Italian")<<QString("it_IT")<<QString("non connesso");
        QTest::newRow("English")<<QString("en_US")<<QString("not connected");
    }
    void disconnectedClientIsNeverAddedToLeaseRenewals() {
        QFETCH(QString,locale);QFETCH(QString,expectedStatus);
        struct Restore {QLocale value;~Restore(){QLocale::setDefault(value);}} restore{QLocale()};
        QLocale::setDefault(QLocale(locale));
        SchoolFeatures features(nullptr,false);const auto ci=client();features.beginPolicyResolution({ci},{});
        QVERIFY(features.m_guardTargets.isEmpty());QVERIFY(!features.m_resolution);QVERIFY(ci->networkPolicyStatus().contains(expectedStatus));
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
        dialog->accept();dialog->reject();dialog->accept();QCOMPARE(replies,1);QCOMPARE(worker.finishes,1);QCOMPARE(dialog->result(),int(QDialog::Accepted));
        QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QVERIFY(!features.m_studentDialog);
    }
    void studentNameKeyboardSubmission_data() {
        QTest::addColumn<int>("key");QTest::addColumn<bool>("cancelDefault");
        QTest::newRow("return")<<int(Qt::Key_Return)<<false;
        QTest::newRow("keypad-enter")<<int(Qt::Key_Enter)<<false;
        QTest::newRow("return-after-cancel-default")<<int(Qt::Key_Return)<<true;
        QTest::newRow("keypad-enter-after-cancel-default")<<int(Qt::Key_Enter)<<true;
    }
    void studentNameKeyboardSubmission() {
        QFETCH(int,key);QFETCH(bool,cancelDefault);
        SchoolFeatures features(nullptr,false);int replies=0;IsolatedSchoolWorker worker(replies);
        const auto id=QUuid::createUuid().toString(QUuid::WithoutBraces);
        QVERIFY(features.handleFeatureMessage(worker,FeatureMessage(features.m_attendanceUid,SchoolFeatures::AskName).addArgument(0,id)));
        QPointer<QDialog> dialog=features.m_studentDialog;auto name=dialog->findChild<QLineEdit*>();auto buttons=dialog->findChild<QDialogButtonBox*>();
        QVERIFY(name);QVERIFY(buttons);QTest::qWait(5);
        name->setText("Anna Rossi");
        if(cancelDefault) buttons->button(QDialogButtonBox::Cancel)->setDefault(true);
        name->setFocus();QTest::keyClick(name,Qt::Key(key));
        QCOMPARE(replies,1);QCOMPARE(worker.finishes,1);QCOMPARE(worker.lastReply.argument(0).toString(),id);
        QCOMPARE(worker.lastReply.argument(1).toString(),QString("Anna Rossi"));
        QVERIFY(dialog);QCOMPARE(dialog->result(),int(QDialog::Accepted));
        QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QVERIFY(!dialog);
    }
    void emptyOrInvalidNameDoesNotCloseOnEnterButEscapeCancels() {
        SchoolFeatures features(nullptr,false);int replies=0;IsolatedSchoolWorker worker(replies);
        QVERIFY(features.handleFeatureMessage(worker,FeatureMessage(features.m_attendanceUid,SchoolFeatures::AskName).addArgument(0,QUuid::createUuid().toString(QUuid::WithoutBraces))));
        const auto dialog=features.m_studentDialog;auto name=dialog->findChild<QLineEdit*>();auto buttons=dialog->findChild<QDialogButtonBox*>();
        for(const auto& invalid:QStringList{"", "   ",QString("Anna")+QChar(0x202e)}) {
            name->setText(invalid);buttons->button(QDialogButtonBox::Cancel)->setDefault(true);name->setFocus();
            QTest::keyClick(name,Qt::Key_Return);QCOMPARE(replies,0);QVERIFY(dialog->isVisible());
            QTest::keyClick(name,Qt::Key_Enter);QCOMPARE(replies,0);QVERIFY(dialog->isVisible());
        }
        QTest::keyClick(name,Qt::Key_Escape);QCOMPARE(replies,1);QCOMPARE(worker.finishes,1);
        QVERIFY(worker.lastReply.argument(1).toString().isEmpty());QCOMPARE(dialog->result(),int(QDialog::Rejected));
    }
    void cancelButtonKeepsItsKeyboardBehaviour() {
        SchoolFeatures features(nullptr,false);int replies=0;IsolatedSchoolWorker worker(replies);
        QVERIFY(features.handleFeatureMessage(worker,FeatureMessage(features.m_attendanceUid,SchoolFeatures::AskName).addArgument(0,QUuid::createUuid().toString(QUuid::WithoutBraces))));
        auto dialog=features.m_studentDialog;dialog->findChild<QLineEdit*>()->setText("Anna Rossi");
        auto cancel=dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Cancel);cancel->setFocus();
        QTest::keyClick(cancel,Qt::Key_Return);QCOMPARE(replies,1);QVERIFY(worker.lastReply.argument(1).toString().isEmpty());
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
        for(const auto& source:{"fe80::25","::1","::192.0.2.25"}) {agent.process(SchoolDiscovery::encode(first),QHostAddress(source),false);QVERIFY(agent.records().isEmpty());}
        agent.process(SchoolDiscovery::encode(first),QHostAddress("2001:db8::25"),false);QCOMPARE(agent.records().size(),1);QVERIFY(agent.records(SchoolDiscovery::AddressMode::Ipv4Only).isEmpty());
        agent.process(SchoolDiscovery::encode(first),QHostAddress("192.0.2.25"),false);QCOMPARE(agent.records().size(),1);QCOMPARE(agent.records().first().address,QString("192.0.2.25"));
        agent.process(SchoolDiscovery::encode(discoveryPacket("Hello",endpoint,9)),QHostAddress("2001:db8::25"),false);QCOMPARE(agent.records().first().address,QString("192.0.2.25"));
        const auto bye=discoveryPacket("Bye",endpoint,10);agent.process(SchoolDiscovery::encode(bye),QHostAddress("2001:db8::25"),false);QCOMPARE(agent.records().size(),1);
        agent.process(SchoolDiscovery::encode(bye),QHostAddress("192.0.2.25"),false);QVERIFY(agent.records().isEmpty());
    }
    void discoveryIpv4SourcesRemainValidAcrossSubnetsAndIgnoreClaimedAddresses() {
        SchoolDiscovery::Agent agent(true,{},11100,nullptr,false);
        for(const auto& source:{"10.20.30.40","172.24.176.20","192.0.2.25"}) {
            const auto endpoint="urn:uuid:"+QUuid::createUuid().toString(QUuid::WithoutBraces);const auto packet=discoveryPacket("Hello",endpoint);
            agent.process(SchoolDiscovery::encode(packet),QHostAddress(source),true);QVERIFY(agent.m_records.contains(endpoint+QChar(0)+source));QCOMPARE(agent.m_records[endpoint+QChar(0)+source].address,QString(source));
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
    void discoveryAnswersIpv6ProbesAndKeepsIpv4OnlyAsAnOption() {
        SchoolDiscovery::Agent clientAgent(false,{},11100,nullptr,false);SchoolDiscovery::Message probe;probe.action="Probe";
        const auto answer=clientAgent.process(SchoolDiscovery::encode(probe),QHostAddress("2001:db8::25"),false);SchoolDiscovery::Message decoded;QVERIFY(SchoolDiscovery::decode(answer,decoded));QCOMPARE(decoded.action.section('/',-1),QString("ProbeMatches"));
        SchoolDiscovery::Agent masterAgent(true,{},11100,nullptr,false);SchoolDiscovery::Record record;record.endpoint="urn:uuid:"+QUuid::createUuid().toString(QUuid::WithoutBraces);record.address="2001:db8::25";masterAgent.m_records.insert(record.endpoint,record);QCOMPARE(masterAgent.records().size(),1);QVERIFY(masterAgent.records(SchoolDiscovery::AddressMode::Ipv4Only).isEmpty());
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
            agent.process(SchoolDiscovery::encode(reply),QHostAddress("2001:db8::25"),false);QVERIFY(agent.records(SchoolDiscovery::AddressMode::Ipv4Only).isEmpty());
            agent.process(SchoolDiscovery::encode(reply),QHostAddress("172.24.176.20"),false);QCOMPARE(agent.records().size(),1);QCOMPARE(agent.records().first().address,QString("172.24.176.20"));
            auto newer=discoveryPacket("Hello",endpoint,2);newer.instance=reply.instance+1;agent.process(SchoolDiscovery::encode(newer),QHostAddress("2001:db8::25"),false);
            auto ipv4=discoveryPacket("Hello",endpoint,3);agent.process(SchoolDiscovery::encode(ipv4),QHostAddress("172.24.176.21"),false);QCOMPARE(agent.records().first().address,QString("172.24.176.21"));
        }
    }
    void discoveryTableRetainsSelectionAndClosesWithPendingTimers_data() {
        QTest::addColumn<QString>("locale");QTest::addColumn<QString>("expectedHeader");
        QTest::newRow("Italian")<<QString("it_IT")<<QString("Indirizzo rilevato");
        QTest::newRow("English")<<QString("en_US")<<QString("Detected address");
    }
    void discoveryTableRetainsSelectionAndClosesWithPendingTimers() {
        QFETCH(QString,locale);QFETCH(QString,expectedHeader);
        struct Restore {QLocale value;~Restore(){QLocale::setDefault(value);}} restore{QLocale()};
        QLocale::setDefault(QLocale(locale));
        IsolatedSchoolMaster master;SchoolFeatures features(nullptr,false);
        features.m_discovery=new SchoolDiscovery::Agent(true,{},11100,&features,false);
        SchoolDiscovery::Record first;first.endpoint="urn:uuid:00000000-0000-0000-0000-000000000001";first.name="<b>Nome non attendibile</b>";first.address="192.0.2.25";
        SchoolDiscovery::Record second=first;second.endpoint="urn:uuid:00000000-0000-0000-0000-000000000002";second.address="192.0.2.26";
        features.m_discovery->m_records.insert(first.endpoint,first);features.m_discovery->m_records.insert(second.endpoint,second);
        auto ipv6=first;ipv6.endpoint="urn:uuid:00000000-0000-0000-0000-000000000003";ipv6.address="2001:db8::25";features.m_discovery->m_records.insert(ipv6.endpoint,ipv6);
        features.m_discovery->m_localIpv4.insert(QHostAddress("192.168.1.195").toIPv4Address());
        auto self=first;self.endpoint="urn:uuid:00000000-0000-0000-0000-000000000004";self.address="192.168.1.195";features.m_discovery->m_records.insert(self.endpoint,self);
        features.discovery(master,{});QPointer<QDialog> dialog=features.m_discoveryDialog;QVERIFY(dialog);
        auto mode=dialog->findChild<QComboBox*>("schoolDiscoveryAddressMode");QVERIFY(mode);QCOMPARE(mode->currentIndex(),0);mode->setCurrentIndex(1);
        auto table=dialog->findChild<QTableWidget*>();QVERIFY(table);QCOMPARE(table->rowCount(),2);QCOMPARE(table->horizontalHeaderItem(1)->text(),expectedHeader);table->selectRow(1);
        auto original=table->item(0,0);auto timer=dialog->findChild<QTimer*>("schoolDiscoveryRefresh");QVERIFY(timer);timer->start(10);QTest::qWait(25);QCOMPARE(table->item(0,0),original);
        first.name="PC aggiornato";features.m_discovery->m_records.insert(first.endpoint,first);
        QTRY_COMPARE_WITH_TIMEOUT(table->item(0,0)->text(),QString("PC aggiornato"),1000);QCOMPARE(table->currentRow(),1);QCOMPARE(table->item(1,1)->text(),second.address);
        features.m_discovery->m_localIpv4.insert(QHostAddress(second.address).toIPv4Address());
        QTRY_COMPARE_WITH_TIMEOUT(table->rowCount(),1,1000);QCOMPARE(table->currentRow(),-1);QVERIFY(table->selectedItems().isEmpty());
        features.m_discovery->m_localIpv4.remove(QHostAddress(second.address).toIPv4Address());
        QTRY_COMPARE_WITH_TIMEOUT(table->rowCount(),2,1000);QCOMPARE(table->currentRow(),-1);
        dialog->reject();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QVERIFY(!dialog);QTest::qWait(20);
    }
    void discoveryKnownHostsUseAddressesAndAsyncDnsNotClaimedNames() {
        FakeDns dns;
        SchoolDiscoveryKnownHosts known({" 192.0.2.25 ","::ffff:192.0.2.26","2001:db8::1",
            "192.0.2.27:11100","[::ffff:192.0.2.28]:11100",
            "Dell.Example.","dell.example:11100","other.invalid"},nullptr,dns.lookup(),dns.abort(),500);
        QVERIFY(known.contains("192.0.2.25"));QVERIFY(known.contains("::ffff:192.0.2.25"));
        QVERIFY(known.contains("192.0.2.26"));QVERIFY(known.contains("2001:db8::1"));
        QVERIFY(known.contains("192.0.2.27"));QVERIFY(known.contains("192.0.2.28"));
        QVERIFY(!known.contains("dell.example"));QCOMPARE(known.pendingCount(),2);
        QCOMPARE(dns.requested.size(),0);known.start();known.start();QCOMPARE(dns.requested.size(),2);
        QCOMPARE(dns.requested.value(1),QString("dell.example"));
        QSignalSpy changed(&known,&SchoolDiscoveryKnownHosts::changed);
        dns.reply(1,{QHostAddress("2001:db8::2"),QHostAddress("192.0.2.109"),QHostAddress("::ffff:192.0.2.110")});
        QVERIFY(known.contains("192.0.2.109"));QVERIFY(known.contains("192.0.2.110"));
        QCOMPARE(known.pendingCount(),1);QCOMPARE(changed.count(),1);
        dns.reply(2,{},QHostInfo::HostNotFound);QCOMPARE(known.pendingCount(),0);QCOMPARE(known.unresolvedCount(),1);
        dns.reply(2,{QHostAddress("192.0.2.99")});QVERIFY(!known.contains("192.0.2.99"));
        known.rememberAddress("192.0.2.99");QVERIFY(known.contains("192.0.2.99"));
        const auto count=changed.count();known.rememberAddress("192.0.2.99");QCOMPARE(changed.count(),count);
    }
    void discoveryKnownHostsBoundConcurrencyAndPreserveMatchesOnTimeout() {
        FakeDns dns;QStringList hosts;
        for(int i=0;i<20;++i) hosts.append(QString("pc%1.invalid").arg(i));
        SchoolDiscoveryKnownHosts known(hosts,nullptr,dns.lookup(),dns.abort(),25);known.start();
        QCOMPARE(dns.requested.size(),8);QCOMPARE(known.pendingCount(),20);
        dns.reply(1,{QHostAddress("192.0.2.25")});QCOMPARE(dns.requested.size(),9);QCOMPARE(known.pendingCount(),19);
        QTRY_COMPARE_WITH_TIMEOUT(known.pendingCount(),0,500);QCOMPARE(known.unresolvedCount(),19);
        QCOMPARE(dns.aborted.size(),8);QVERIFY(known.contains("192.0.2.25"));
        dns.reply(2,{QHostAddress("192.0.2.26")});QVERIFY(!known.contains("192.0.2.26"));
        known.start();QCOMPARE(dns.requested.size(),9);
    }
    void discoveryKnownHostsAcceptIpv6OnlyAndKeepFailedNamesUnmatched() {
        FakeDns dns;
        SchoolDiscoveryKnownHosts known({"one.invalid","two.invalid"},nullptr,dns.lookup(),dns.abort(),500);
        known.start();dns.reply(1,{QHostAddress("2001:db8::25")});dns.reply(2,{},QHostInfo::UnknownError);
        QCOMPARE(known.pendingCount(),0);QCOMPARE(known.unresolvedCount(),1);QVERIFY(!known.contains("192.0.2.25"));QVERIFY(known.contains("2001:db8::25"));
        SchoolDiscoveryKnownHosts failed({"failed.invalid"},nullptr,
            [](const QString&,QObject*,const Callback&){return -1;},dns.abort(),500);
        failed.start();QCOMPARE(failed.pendingCount(),0);QCOMPARE(failed.unresolvedCount(),1);
    }
    void discoveryKnownHostsOwnCancellationAndRefreshDhcpOnReopen() {
        FakeDns dns;
        auto known=new SchoolDiscoveryKnownHosts({"dell.example"},nullptr,dns.lookup(),dns.abort(),500);
        known->start();delete known;QCOMPARE(dns.aborted,QList<int>{1});
        SchoolDiscoveryKnownHosts first({"dell.example"},nullptr,dns.lookup(),dns.abort(),500);
        first.start();dns.reply(2,{QHostAddress("192.0.2.109")});QVERIFY(first.contains("192.0.2.109"));
        SchoolDiscoveryKnownHosts reopened({"dell.example"},nullptr,dns.lookup(),dns.abort(),500);
        reopened.start();dns.reply(3,{QHostAddress("192.0.2.110")});
        QVERIFY(reopened.contains("192.0.2.110"));QVERIFY(!reopened.contains("192.0.2.109"));
    }
    void discoveryTableHidesKnownAndPreservesSelectionWithoutGuessingNames_data() {
        QTest::addColumn<QString>("locale");QTest::addColumn<QString>("knownStatus");
        QTest::newRow("Italian")<<QString("it_IT")<<QString("Già configurato");
        QTest::newRow("English")<<QString("en_US")<<QString("Already configured");
    }
    void discoveryTableHidesKnownAndPreservesSelectionWithoutGuessingNames() {
        QFETCH(QString,locale);QFETCH(QString,knownStatus);
        struct Restore {QLocale value;~Restore(){QLocale::setDefault(value);}} restore{QLocale()};
        QLocale::setDefault(QLocale(locale));
        IsolatedSchoolMaster master;SchoolFeatures features(nullptr,false);
        features.m_discovery=new SchoolDiscovery::Agent(true,{},11100,&features,false);
        for(int i=1;i<=3;++i) {
            SchoolDiscovery::Record record;record.endpoint=QString("urn:uuid:00000000-0000-0000-0000-%1").arg(i,12,10,QLatin1Char('0'));
            record.name="Same claimed name";record.address=QString("192.0.2.%1").arg(24+i);
            features.m_discovery->m_records.insert(record.endpoint,record);
        }
        features.discovery(master,{"192.0.2.25","192.0.2.26"});
        QPointer<QDialog> dialog=features.m_discoveryDialog;QVERIFY(dialog);
        auto table=dialog->findChild<QTableWidget*>();auto filter=dialog->findChild<QCheckBox*>("schoolDiscoveryHideKnown");
        auto summary=dialog->findChild<QLabel*>("schoolDiscoverySummary");
        QVERIFY(table);QVERIFY(filter);QVERIFY(summary);QVERIFY(filter->isChecked());
        QCOMPARE(table->horizontalHeaderItem(3)->text(),locale=="it_IT"?QString("Posizioni e computer"):QString("Locations & computers"));
        QCOMPARE(table->rowCount(),1);QCOMPARE(table->item(0,1)->text(),QString("192.0.2.27"));
        QVERIFY(summary->text().contains("1"));QVERIFY(summary->text().contains("3"));QVERIFY(summary->text().contains("2"));
        table->selectRow(0);filter->setChecked(false);QCOMPARE(table->rowCount(),3);
        QCOMPARE(table->currentRow(),2);QCOMPARE(table->item(0,3)->text(),knownStatus);
        QCOMPARE(table->item(1,3)->text(),knownStatus);QVERIFY(table->item(2,3)->text()!=knownStatus);
        // Re-showing known peers must not make importing a duplicate possible.
        table->selectRow(0);auto buttons=dialog->findChild<QDialogButtonBox*>();QVERIFY(buttons);
        QPushButton* import=nullptr;
        for(auto button:buttons->buttons()) if(buttons->buttonRole(button)==QDialogButtonBox::ActionRole) import=qobject_cast<QPushButton*>(button);
        QVERIFY(import);import->click();auto notice=dialog->findChild<QMessageBox*>();QVERIFY(notice);
        QCOMPARE(notice->text(),NetLinkUi::text("Computer already configured."));notice->accept();
        table->selectRow(2);
        const auto output=qEnvironmentVariable("NETLINK_DISCOVERY_SCREENSHOTS");
        if(!output.isEmpty()) {QVERIFY(QDir().mkpath(output));QVERIFY(dialog->grab().save(QDir(output).filePath(QString("discovery-%1-%2.png").arg(locale).arg(qRound(dialog->devicePixelRatioF()*100)))));}
        filter->setChecked(true);QCOMPARE(table->rowCount(),1);QCOMPARE(table->currentRow(),0);
        filter->setChecked(false);table->selectRow(0);filter->setChecked(true);
        QCOMPARE(table->currentRow(),-1);QVERIFY(table->selectedItems().isEmpty());
        auto known=dialog->findChild<SchoolDiscoveryKnownHosts*>();QVERIFY(known);
        known->rememberAddress("192.0.2.27");QCOMPARE(table->rowCount(),0);
        filter->setChecked(false);QCOMPARE(table->rowCount(),3);QCOMPARE(table->currentRow(),-1);
        QCOMPARE(table->item(2,3)->text(),knownStatus);
        dialog->reject();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QVERIFY(!dialog);
    }
    void discoveryTableUpdatesAfterDnsAndCancelsLookupsOnClose() {
        FakeDns dns;IsolatedSchoolMaster master;SchoolFeatures features(nullptr,false);
        features.m_discovery=new SchoolDiscovery::Agent(true,{},11100,&features,false);
        for(int i=1;i<=2;++i) {
            SchoolDiscovery::Record record;record.endpoint=QString::number(i);record.name="dell.example";
            record.address=QString("192.0.2.%1").arg(24+i);features.m_discovery->m_records.insert(record.endpoint,record);
        }
        features.discovery(master,{"dell.example"});
        QPointer<QDialog> dialog=features.m_discoveryDialog;auto known=dialog->findChild<SchoolDiscoveryKnownHosts*>();
        QVERIFY(known);known->m_lookup=dns.lookup();known->m_abort=dns.abort();
        auto table=dialog->findChild<QTableWidget*>();QCOMPARE(table->rowCount(),2);table->selectRow(1);
        QTRY_COMPARE_WITH_TIMEOUT(dns.requested.size(),1,500);QCOMPARE(dns.requested.value(1),QString("dell.example"));
        dns.reply(1,{QHostAddress("192.0.2.25")});QCOMPARE(table->rowCount(),1);
        QCOMPARE(table->item(0,1)->text(),QString("192.0.2.26"));QCOMPARE(table->currentRow(),0);
        dialog->reject();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QVERIFY(!dialog);
        features.discovery(master,{"dell.example"});dialog=features.m_discoveryDialog;
        known=dialog->findChild<SchoolDiscoveryKnownHosts*>();known->m_lookup=dns.lookup();known->m_abort=dns.abort();
        QTRY_COMPARE_WITH_TIMEOUT(dns.requested.size(),2,500);
        dialog->reject();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QVERIFY(!dialog);
        QCOMPARE(dns.aborted,QList<int>{2});
    }
    void discoveryCanonicalAddressesKeepLocalScopesAndRejectUnsafeDestinations() {
        using SchoolDiscovery::addressKey;
        QCOMPARE(addressKey(QHostAddress("::ffff:192.0.2.25")),QString("192.0.2.25"));
        QCOMPARE(addressKey(QHostAddress("2001:0db8:0:0::25%17")),QString("2001:db8::25"));
        QCOMPARE(addressKey(QHostAddress("fe80::25%99"),17),QString("fe80::25%17"));
        QCOMPARE(addressKey(QHostAddress("fe80::25%17")),QString("fe80::25%17"));
        QVERIFY(addressKey(QHostAddress("fe80::25")).isEmpty());
        for(const auto& value:{"0.0.0.0","::","::1","127.0.0.1","ff02::c","224.0.0.1","255.255.255.255","::192.0.2.25"}) QVERIFY(addressKey(QHostAddress(value)).isEmpty());
    }
    void discoveryAutomaticDeduplicatesBothFamiliesRegardlessOfArrivalOrder() {
        for(bool ipv6First:{false,true}) {
            SchoolDiscovery::Agent agent(true,{},11100,nullptr,false);
            const auto endpoint="urn:uuid:"+QUuid::createUuid().toString(QUuid::WithoutBraces);
            const auto packet=discoveryPacket("Hello",endpoint);const auto bytes=SchoolDiscovery::encode(packet);
            for(const auto& source:ipv6First?QStringList{"2001:db8::25","192.0.2.25"}:QStringList{"192.0.2.25","2001:db8::25"}) {
                agent.process(bytes,QHostAddress(source),false);agent.process(bytes,QHostAddress(source),false);
            }
            QCOMPARE(agent.m_records.size(),2);QCOMPARE(agent.records().size(),1);
            const auto record=agent.records().first();QCOMPARE(record.address,QString("192.0.2.25"));
            QCOMPARE(record.addresses,QStringList({"192.0.2.25","2001:db8::25"}));QVERIFY(!record.conflict);
            QCOMPARE(agent.records(SchoolDiscovery::AddressMode::Ipv4Only).size(),1);
            // Advertised metadata must not become an observed alias.
            QVERIFY(!record.addresses.contains("198.51.100.99"));
        }
    }
    void discoveryAutomaticFallsBackAfterExpiryAndByeOnlyRemovesItsOwnAddress() {
        SchoolDiscovery::Agent agent(true,{},11100,nullptr,false);const auto endpoint="urn:uuid:"+QUuid::createUuid().toString(QUuid::WithoutBraces);
        auto packet=discoveryPacket("Hello",endpoint);
        agent.process(SchoolDiscovery::encode(packet),QHostAddress("192.0.2.25"),false);
        agent.process(SchoolDiscovery::encode(packet),QHostAddress("2001:db8::25"),false);
        const auto v4=endpoint+QChar(0)+"192.0.2.25";agent.m_records[v4].seen=-96000;
        QCOMPARE(agent.records().first().address,QString("2001:db8::25"));QVERIFY(agent.records(SchoolDiscovery::AddressMode::Ipv4Only).isEmpty());
        agent.m_records[v4].seen=agent.m_clock.elapsed();
        agent.process(SchoolDiscovery::encode(discoveryPacket("Bye",endpoint,2)),QHostAddress("2001:db8::25"),false);
        QCOMPARE(agent.records().size(),1);QCOMPARE(agent.records().first().address,QString("192.0.2.25"));
        agent.process(SchoolDiscovery::encode(discoveryPacket("Hello",endpoint,3)),QHostAddress("2001:db8::25"),false);
        agent.process(SchoolDiscovery::encode(discoveryPacket("Bye",endpoint,2)),QHostAddress("192.0.2.25"),false);
        QCOMPARE(agent.records().size(),1);QCOMPARE(agent.records().first().address,QString("2001:db8::25"));
    }
    void discoveryIpv6ScopeComesFromReceiverAndMasterIsExcludedOnAllFamilies() {
        SchoolDiscovery::Agent agent(true,{},11100,nullptr,false);const auto endpoint="urn:uuid:"+QUuid::createUuid().toString(QUuid::WithoutBraces);
        auto packet=discoveryPacket("Hello",endpoint);packet.records.first().address="fe80::999%99";
        agent.process(SchoolDiscovery::encode(packet),QHostAddress("fe80::25"),false);QVERIFY(agent.records().isEmpty());
        agent.process(SchoolDiscovery::encode(packet),QHostAddress("fe80::25%99"),false,17);
        QCOMPARE(agent.records().first().address,QString("fe80::25%17"));
        QVERIFY(!agent.records().first().addresses.contains("fe80::999%99"));
        agent.m_localIpv6.insert("fe80::25%17");QVERIFY(agent.records().isEmpty());
        agent.process(SchoolDiscovery::encode(discoveryPacket("Hello",endpoint,2)),QHostAddress("2001:db8::25"),false);
        QCOMPARE(agent.records().first().address,QString("2001:db8::25"));
        agent.m_localIpv6.insert("2001:db8::25");QVERIFY(agent.records().isEmpty());
        // A copied EPR or hostname alone must not hide a remote address.
        auto remote=discoveryPacket("Hello",agent.m_endpoint,3);remote.records.first().name=QHostInfo::localHostName();
        agent.process(SchoolDiscovery::encode(remote),QHostAddress("2001:db8::26"),false);QCOMPARE(agent.records().size(),1);
    }
    void discoveryConflictingIdentityStaysOneVisibleUntrustedRowAndIsNotImported() {
        IsolatedSchoolMaster master;SchoolFeatures features(nullptr,false);
        features.m_discovery=new SchoolDiscovery::Agent(true,{},11100,&features,false);auto& agent=*features.m_discovery;
        const auto endpoint="urn:uuid:"+QUuid::createUuid().toString(QUuid::WithoutBraces);
        auto first=discoveryPacket("Hello",endpoint);agent.process(SchoolDiscovery::encode(first),QHostAddress("192.0.2.25"),false);
        first.id=QUuid::createUuid().toString();first.records.first().name="Different claimed PC";first.records.first().port=11101;
        agent.process(SchoolDiscovery::encode(first),QHostAddress("2001:db8::25"),false);
        QCOMPARE(agent.records().size(),1);QVERIFY(agent.records().first().conflict);
        features.discovery(master,{"192.0.2.25"});auto dialog=features.m_discoveryDialog;
        auto table=dialog->findChild<QTableWidget*>();QCOMPARE(table->rowCount(),1);table->selectRow(0);
        auto buttons=dialog->findChild<QDialogButtonBox*>();for(auto button:buttons->buttons()) if(buttons->buttonRole(button)==QDialogButtonBox::ActionRole) button->click();
        auto notice=dialog->findChild<QMessageBox*>();QVERIFY(notice);QVERIFY(notice->text().contains(NetLinkUi::text("Discovery results changed or conflict. Select the computer again or add it manually.")));
    }
    void discoveryBoundsObservationsAndDoesNotMergeByClaimedName() {
        SchoolDiscovery::Agent agent(true,{},11100,nullptr,false);const auto endpoint="urn:uuid:"+QUuid::createUuid().toString(QUuid::WithoutBraces);
        for(int i=1;i<=32;++i) agent.process(SchoolDiscovery::encode(discoveryPacket("Hello",endpoint)),QHostAddress(QString("2001:db8::%1").arg(i)),false);
        QCOMPARE(agent.m_records.size(),8);QCOMPARE(agent.records().size(),1);QCOMPARE(agent.records().first().addresses.size(),8);
        agent.process(SchoolDiscovery::encode(discoveryPacket("Hello",endpoint)),QHostAddress("192.0.2.24"),false);
        QCOMPARE(agent.records().size(),1);QCOMPARE(agent.records().first().address,QString("192.0.2.24"));
        const auto other="urn:uuid:"+QUuid::createUuid().toString(QUuid::WithoutBraces);
        agent.process(SchoolDiscovery::encode(discoveryPacket("Hello",other)),QHostAddress("192.0.2.25"),false);QCOMPARE(agent.records().size(),2);
    }
    void discoveryKnownFilterMatchesIpv6AliasesAndKeepsLinkLocalZonesDistinct() {
        FakeDns dns;SchoolDiscoveryKnownHosts known({"2001:0db8::25","[fe80::25%17]:11100","dell.example"},nullptr,dns.lookup(),dns.abort(),500);
        QVERIFY(known.contains("2001:db8::25"));QVERIFY(known.contains("fe80::25%17"));QVERIFY(!known.contains("fe80::25%18"));QVERIFY(!known.contains("fe80::25"));
        SchoolDiscovery::Record record;record.address="192.0.2.25";record.addresses=QStringList{record.address,"2001:db8::25"};
        QVERIFY(known.contains(record));record.conflict=true;QVERIFY(!known.contains(record));
        known.start();dns.reply(1,{QHostAddress("2001:db8::26"),QHostAddress("192.0.2.26")});
        QVERIFY(known.contains("2001:db8::26"));QVERIFY(known.contains("192.0.2.26"));QCOMPARE(known.unresolvedCount(),0);
    }
    void discoveryModeAndKnownFilterPreserveOneRowAndPersonalPreference() {
        IsolatedSchoolMaster master;SchoolFeatures features(nullptr,false);
        features.m_discovery=new SchoolDiscovery::Agent(true,{},11100,&features,false);auto& agent=*features.m_discovery;
        const auto endpoint="urn:uuid:"+QUuid::createUuid().toString(QUuid::WithoutBraces);auto packet=discoveryPacket("Hello",endpoint);
        agent.process(SchoolDiscovery::encode(packet),QHostAddress("192.0.2.25"),false);agent.process(SchoolDiscovery::encode(packet),QHostAddress("2001:db8::25"),false);
        const auto ipv6Only="urn:uuid:"+QUuid::createUuid().toString(QUuid::WithoutBraces);
        agent.process(SchoolDiscovery::encode(discoveryPacket("Hello",ipv6Only)),QHostAddress("2001:db8::26"),false);
        features.discovery(master,{"2001:db8::25"});QPointer<QDialog> dialog=features.m_discoveryDialog;
        auto table=dialog->findChild<QTableWidget*>();auto mode=dialog->findChild<QComboBox*>("schoolDiscoveryAddressMode");auto filter=dialog->findChild<QCheckBox*>("schoolDiscoveryHideKnown");
        QCOMPARE(mode->currentIndex(),0);QCOMPARE(table->rowCount(),1);QCOMPARE(table->item(0,1)->text(),QString("2001:db8::26"));
        filter->setChecked(false);QCOMPARE(table->rowCount(),2);mode->setCurrentIndex(1);QCOMPARE(table->rowCount(),1);
        QCOMPARE(table->item(0,1)->text(),QString("192.0.2.25"));QVERIFY(table->item(0,1)->toolTip().contains("2001:db8::25"));
        QCOMPARE(master.preferences.value("AddressMode","SchoolDiscovery",{}).toString(),QString("IPv4Only"));
        table->selectRow(0);filter->setChecked(true);QCOMPARE(table->rowCount(),0);QCOMPARE(table->currentRow(),-1);
        dialog->reject();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QVERIFY(!dialog);
        features.discovery(master,{});dialog=features.m_discoveryDialog;
        mode=dialog->findChild<QComboBox*>("schoolDiscoveryAddressMode");QCOMPARE(mode->currentIndex(),1);
        mode->setCurrentIndex(0);QCOMPARE(dialog->findChild<QTableWidget*>()->rowCount(),2);
        table=dialog->findChild<QTableWidget*>();
        const auto output=qEnvironmentVariable("NETLINK_DISCOVERY_SCREENSHOTS");
        if(!output.isEmpty()) {QVERIFY(QDir().mkpath(output));QVERIFY(dialog->grab().save(QDir(output).filePath(QString("discovery-dual-%1-%2.png").arg(QLocale().name()).arg(qRound(dialog->devicePixelRatioF()*100)))));}
        // The exact validator used before and after import confirmation must
        // reject a pending IPv6 choice after switching to IPv4 only. Keep this
        // fixture free of production configuration, registry and key access.
        SchoolDiscovery::Record selected;
        for(const auto& record:agent.records()) if(record.endpoint==ipv6Only) selected=record;
        QVERIFY(agent.isCurrent(selected,SchoolDiscovery::AddressMode::Automatic));
        QVERIFY(!agent.isCurrent(selected,SchoolDiscovery::AddressMode::Ipv4Only));
        selected.name="Changed name";QVERIFY(!agent.isCurrent(selected,SchoolDiscovery::AddressMode::Automatic));
    }
};
int main(int argc,char** argv) {
    QApplication app(argc,argv);const auto arguments=app.arguments();
    if(arguments.size()==4 && arguments[1]=="--attendance-child") return SchoolLifecycleTests::attendanceChild(arguments[2].toUShort(),arguments[3]);
    SchoolLifecycleTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "SchoolLifecycleTests.moc"
