// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025-26 prof. Ing. Raffaele Mele
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include <QtTest>
#include "NetLinkIdentity.h"
#include "Filesystem.h"
#include "VeyonServiceControl.h"
#include "SchoolNetworkPolicy.h"

class ApplicationIdentityTests : public QObject {
    Q_OBJECT
    using App=NetLinkIdentity::Application;
private Q_SLOTS:
    void everyApplicationHasTheExpectedName() {
        const QList<App> apps{App::Master,App::Configurator,App::Service,App::Server,App::Worker,App::Cli,App::WCli,App::Companion};
        const QStringList parts{"master","configurator","service","server","worker","cli","wcli","companion"};
        for(int i=0;i<apps.size();++i) {
#ifdef Q_OS_WIN
            QCOMPARE(NetLinkIdentity::executableName(apps[i]),"netlink-"+parts[i]+".exe");
#else
            QCOMPARE(NetLinkIdentity::executableName(apps[i]),(apps[i]==App::Companion?"netlink-":"veyon-")+parts[i]);
#endif
        }
        const auto invalid=static_cast<App>(999);QVERIFY(NetLinkIdentity::executableName(invalid).isEmpty());QVERIFY(NetLinkIdentity::executablePath(invalid).isEmpty());
    }
    void localLaunchPathsUseTheCurrentNames() {
        Filesystem filesystem;
        QCOMPARE(filesystem.serverFilePath(),NetLinkIdentity::executablePath(App::Server));
        QCOMPARE(filesystem.workerFilePath(),NetLinkIdentity::executablePath(App::Worker));
        QCOMPARE(VeyonServiceControl::filePath(),NetLinkIdentity::executablePath(App::Service));
        for(auto app:{App::Server,App::Worker,App::Service}) {
            QCOMPARE(QDir::fromNativeSeparators(NetLinkIdentity::executablePath(app)),QCoreApplication::applicationDirPath()+"/"+NetLinkIdentity::executableName(app));
        }
        QCOMPARE(QString::fromLatin1(SchoolNetwork::PipeName),QString("VeyonSchool.NetworkPolicy.v1"));
    }
    void strictPeerPathsNeverAcceptLegacyAliasesOrOtherDirectories() {
        const auto directory=QCoreApplication::applicationDirPath();
        for(auto app:{App::Service,App::Server}) {
            const auto name=NetLinkIdentity::executableName(app),path=QDir(directory).filePath(name);
            QVERIFY(NetLinkIdentity::matchesInstalledPath(path,directory,app));
            QVERIFY(NetLinkIdentity::matchesInstalledPath(QDir::toNativeSeparators(path),directory,app));
            QVERIFY(!NetLinkIdentity::matchesInstalledPath(name,directory,app));
            QVERIFY(!NetLinkIdentity::matchesInstalledPath(QDir(directory+"/other").filePath(name),directory,app));
            QVERIFY(!NetLinkIdentity::matchesInstalledPath(path+".evil",directory,app));
            QVERIFY(!NetLinkIdentity::matchesInstalledPath(path,"relative",app));
#ifdef Q_OS_WIN
            QVERIFY(NetLinkIdentity::matchesInstalledPath(path.toUpper(),directory,app));
            QVERIFY(!NetLinkIdentity::matchesInstalledPath(QDir(directory).filePath("veyon-"+NetLinkIdentity::component(app)+".exe"),directory,app));
#endif
        }
    }
    void diagnosticNamesAcceptNewAndOldReportsButNotArbitraryPrograms() {
        for(const auto& part:{"master","configurator","service","server","worker","cli","wcli"}) {
            QVERIFY(NetLinkIdentity::isDiagnosticApplication("netlink-"+QString(part)+".exe"));
            QVERIFY(NetLinkIdentity::isDiagnosticApplication("VEYON-"+QString(part).toUpper()+".EXE"));
        }
        QVERIFY(NetLinkIdentity::isDiagnosticApplication("netlink-companion.exe"));
        for(const auto& name:{"veyon-companion.exe","netlink-other.exe","netlink-server.exe.evil","C:/other/netlink-server.exe","unrelated.exe"})
            QVERIFY(!NetLinkIdentity::isDiagnosticApplication(name));
    }
};
QTEST_GUILESS_MAIN(ApplicationIdentityTests)
#include "ApplicationIdentityTests.moc"
