// SPDX-License-Identifier: GPL-2.0-or-later
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include <QtTest>
#include <QtCrypto>
#include "DemoFeaturePlugin.h"
#include "SelectedFeatureControl.h"
#include "VeyonMasterInterface.h"

class DemoTestMaster : public VeyonMasterInterface {
public:
    DemoTestMaster() : VeyonMasterInterface(nullptr),local(Computer()) {}
    QWidget* mainWindow() override {return &window;}
    Configuration::Object* userConfigurationObject() override {return nullptr;}
    void reloadSubFeatures() override {}
    ComputerControlInterface& localSessionControlInterface() override {return local;}
    const ComputerControlInterfaceList& allComputerControlInterfaces() const override {return clients;}
    ComputerControlInterfaceList selectedComputerControlInterfaces() const override {return selected;}
    ComputerControlInterfaceList filteredComputerControlInterfaces() const override {return clients;}
    QWidget window;ComputerControlInterface local;ComputerControlInterfaceList clients,selected;
};

// In-memory configuration, disconnected control interfaces, no VeyonCore runtime,
// service, socket listener, production settings or keys are used by this fixture.
class DemoSelectionTests : public QObject {
    Q_OBJECT
    QCA::Initializer crypto;
    static ComputerControlInterface::Pointer client() {
        return ComputerControlInterface::Pointer::create(Computer(QUuid::createUuid(),"PC","192.0.2.1",{},{}));
    }
    static QVariantMap clientArguments() {return {{"demoServerHost","192.0.2.5"},{"demoServerPort",11101}};}
private Q_SLOTS:
    void stoppingSubsetKeepsOtherRecipientsAndServer() {
        Configuration::Object configuration;DemoFeaturePlugin plugin(nullptr,&configuration);DemoTestMaster master;
        plugin.m_configuration.setSlowDownThumbnailUpdates(false);
        const auto first=client(),second=client();
        QVERIFY(plugin.controlDemoClient(plugin.m_demoClientWindowFeature.uid(),FeatureProviderInterface::Operation::Start,clientArguments(),{first,second}));
        QCOMPARE(plugin.m_demoServerClients.size(),2);
        plugin.controlFeature(plugin.m_demoServerFeature.uid(),FeatureProviderInterface::Operation::Start,{{"demoServerPort",11101},{"vncServerPortOffset",0}},{master.local.weakPointer()});
        const auto token=plugin.m_demoAccessToken.toByteArray();
        QVERIFY(plugin.stopFeature(master,plugin.m_demoFeature,{first}));
        QCOMPARE(plugin.m_demoServerClients,ComputerControlInterfaceList{second});
        QVERIFY(plugin.m_demoServerControlTimer.isActive());QCOMPARE(plugin.m_demoAccessToken.toByteArray(),token);
        QVERIFY(plugin.stopFeature(master,plugin.m_demoFeature,{second}));
        QVERIFY(plugin.m_demoServerClients.isEmpty());QVERIFY(!plugin.m_demoServerControlTimer.isActive());
        QVERIFY(plugin.m_demoAccessToken.toByteArray()!=token);
    }
    void localPreviewDoesNotPreventLastRecipientCleanup() {
        Configuration::Object configuration;DemoFeaturePlugin plugin(nullptr,&configuration);DemoTestMaster master;
        plugin.m_configuration.setSlowDownThumbnailUpdates(false);
        const auto first=client();const auto local=master.local.weakPointer();
        plugin.controlDemoClient(plugin.m_demoClientWindowFeature.uid(),FeatureProviderInterface::Operation::Start,clientArguments(),{first,local});
        QVERIFY(plugin.stopFeature(master,plugin.m_demoFeature,{first}));
        QVERIFY(plugin.m_demoServerClients.isEmpty());QVERIFY(!plugin.m_demoServerControlTimer.isActive());
    }
    void reconnectReusesOnlyThatRecipientAndDoesNotAskForSource() {
        Configuration::Object configuration;DemoFeaturePlugin plugin(nullptr,&configuration);DemoTestMaster master;
        plugin.m_configuration.setSlowDownThumbnailUpdates(false);
        const auto first=client(),second=client();
        first->setProperty("netlinkDemoRecipientArguments",clientArguments());
        plugin.controlDemoClient(plugin.m_demoClientWindowFeature.uid(),FeatureProviderInterface::Operation::Start,clientArguments(),{first,second});
        QVERIFY(plugin.startFeature(master,plugin.m_shareUserScreenWindowFeature,{first}));
        QCOMPARE(plugin.m_demoServerClients,ComputerControlInterfaceList({first,second}));
        QVERIFY(plugin.stopFeature(master,plugin.m_demoFeature,{first,second}));
    }
    void changingSourceCannotDisruptUnselectedRecipients() {
        const auto first=client(),second=client();
        first->setProperty("netlinkDemoOrigin","local");second->setProperty("netlinkDemoOrigin","another-source");
        const ComputerControlInterfaceList all{first,second};
        QVERIFY(SelectedFeatureControl::conflictingDemoSource(all,{first},"local"));
        QVERIFY(!SelectedFeatureControl::conflictingDemoSource(all,{first},"another-source"));
        QVERIFY(!SelectedFeatureControl::conflictingDemoSource(all,all,"local"));
    }
};
QTEST_MAIN(DemoSelectionTests)
#include "DemoSelectionTests.moc"
