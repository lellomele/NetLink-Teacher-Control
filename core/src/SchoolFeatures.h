// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QDialog>
#include <QElapsedTimer>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <QThread>
#include <QJsonArray>
#include <functional>
#include "FeatureProviderInterface.h"
#include "ConfigurationPagePluginInterface.h"
#include "CommandLinePluginInterface.h"
#include "SchoolDiscovery.h"

class SchoolPolicyResolver;

class VEYON_CORE_EXPORT SchoolFeatures : public QObject, public PluginInterface,
    public FeatureProviderInterface, public ConfigurationPagePluginInterface, public CommandLinePluginInterface {
    Q_OBJECT
    Q_INTERFACES(PluginInterface FeatureProviderInterface ConfigurationPagePluginInterface CommandLinePluginInterface)
public:
    explicit SchoolFeatures(QObject* parent = nullptr);
    ~SchoolFeatures() override;
    Plugin::Uid uid() const override { return QUuid(QStringLiteral("fa8146e2-bec1-4bdb-a620-c46335d27137")); }
    QVersionNumber version() const override { return QVersionNumber(0,1,0); }
    QString name() const override { return QStringLiteral("NetLink"); }
    QString description() const override { return QStringLiteral("InternetGuard WFP, Appello, WS-Discovery"); }
    QString vendor() const override { return QStringLiteral("prof. Ing. Raffaele Mele"); }
    QString copyright() const override { return QStringLiteral("© 2026 prof. Ing. Raffaele Mele"); }
    const FeatureList& featureList() const override { return m_features; }
    ConfigurationPage* createConfigurationPage() override;
    bool controlFeature(Feature::Uid, Operation, const QVariantMap&, const ComputerControlInterfaceList&) override;
    bool startFeature(VeyonMasterInterface&, const Feature&, const ComputerControlInterfaceList&) override;
    bool handleFeatureMessage(ComputerControlInterface::Pointer, const FeatureMessage&) override;
    bool handleFeatureMessage(VeyonServerInterface&, const MessageContext&, const FeatureMessage&) override;
    bool handleFeatureMessage(VeyonWorkerInterface&, const FeatureMessage&) override;
    int activeInternetPolicies() const {
        QSet<const ComputerControlInterface*> active;
        for(const auto& target:m_guardTargets) if(target.control) active.insert(target.control.data());
        for(const auto& ci:m_resolutionClients) if(ci) active.insert(ci.data());
        for(const auto& ci:m_unconfirmedReleases) if(ci) active.insert(ci.data());
        return active.size();
    }
    void stopPolicyRenewals() { m_timer.stop(); }
    QString commandLineModuleName() const override {return QStringLiteral("school");}
    QString commandLineModuleHelp() const override {return QStringLiteral("NetLink discovery firewall");}
    QStringList commands() const override {return {QStringLiteral("firewall_setup"),QStringLiteral("firewall_remove"),QStringLiteral("selftest")};}
    QString commandHelp(const QString& command) const override {return command;}
public Q_SLOTS:
    CommandLinePluginInterface::RunResult handle_firewall_setup(const QStringList&);
    CommandLinePluginInterface::RunResult handle_firewall_remove(const QStringList&);
    CommandLinePluginInterface::RunResult handle_selftest(const QStringList&);
private:
    friend class SchoolLifecycleTests;
    friend class ClassProfileTests;
    SchoolFeatures(QObject* parent, bool initializeRuntime);
    enum Command { Apply=1, Release=2, Status=3, Reply=4, AskName=5, NameReply=6, EndAttendance=7 };
    void policy(const ComputerControlInterfaceList&, bool block);
    void sendPolicy(ComputerControlInterface*, int command, const QStringList& addresses, int retry = 0);
    void tick();
    void attendance(VeyonMasterInterface&, const ComputerControlInterfaceList&);
    void discovery(VeyonMasterInterface&);
    void discovery(VeyonMasterInterface&, const QStringList& configuredHosts);
    // The private isolated constructor leaves these callbacks inert. Tests can
    // exercise the complete import flow without HKLM, services or real keys.
    struct DiscoveryImportContext {
        std::function<QJsonArray()> objects = [] {return QJsonArray{};};
        std::function<int()> port = [] {return 11100;};
        std::function<bool()> writable = [] {return false;};
        std::function<bool(const QJsonArray&,const QJsonArray&,QString&)> save =
            [](const QJsonArray&,const QJsonArray&,QString&) {return false;};
    } m_discoveryImport;
    void beginPolicyResolution(const ComputerControlInterfaceList&, const QStringList& resources);
    FeatureList m_features;
    const QUuid m_guardUid, m_attendanceUid, m_discoveryUid;
    QTimer m_timer;
    QTimer m_attendanceTimer;
    SchoolDiscovery::Agent* m_discovery = nullptr;
    QThread* m_serviceThread = nullptr;
    struct Target { QPointer<ComputerControlInterface> control; QStringList addresses; qint64 sent = 0; };
    QList<Target> m_guardTargets;
    QList<QPointer<ComputerControlInterface>> m_unconfirmedReleases;
    QElapsedTimer m_clock;
    MessageContext m_attendanceContext;
    QString m_attendanceRequest;
    QPointer<QDialog> m_studentDialog;
    QPointer<QDialog> m_guardDialog, m_attendanceDialog, m_discoveryDialog;
    QPointer<SchoolPolicyResolver> m_resolution;
    ComputerControlInterfaceList m_resolutionClients;
    int m_policyInFlight = 0;
};
