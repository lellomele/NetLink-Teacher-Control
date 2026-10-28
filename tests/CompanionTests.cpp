// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025-26 prof. Ing. Raffaele Mele
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#define NETLINK_COMPANION_WIDGET_TEST
#include "../companion/src/main.cpp"
#include <QtTest>
#include <QSettings>
#include <QTemporaryDir>
#include <QTcpServer>

class CompanionTests : public QObject {
    Q_OBJECT
    QJsonObject fixture(const QString& address="192.0.2.10") {
        return {{"format","netlink-companion"},{"schema",1},{"identifiersIncluded",true},{"system",QJsonObject{{"computer","PC-DIMOSTRATIVO"},{"os","Windows 10"},{"architecture","x86_64"}}},
            {"settings",QJsonObject{{"Authentication/Method",1},{"Network/VeyonServerPort",11100},{"School/DiscoveryMaster","192.0.2.10"}}},
            {"interfaces",QJsonArray{QJsonObject{{"name","Wi-Fi"},{"up",true},{"addresses",QJsonArray{QJsonObject{{"address",address},{"prefix",24}}}},{"dns",QJsonArray{"192.0.2.1"}},{"gateways",QJsonArray{"192.0.2.1"}}}}},
            {"keys",QJsonArray{QJsonObject{{"type","public"},{"name","docente"},{"sha256","abc"}}}},{"services",QJsonArray{QJsonObject{{"name","VeyonSchoolService"},{"state","running"}}}},{"errors",QJsonArray{}}};
    }
private Q_SLOTS:
    void roleSpecificTestsAndProtectedLinkInstructions() {
        auto report=fixture();report["configuredComputers"]=QJsonArray{QJsonObject{{"address","192.0.2.25"},{"name","PC client"}}};CompanionWindow window(report,false);
        window.m_role->setCurrentIndex(1);QCOMPARE(window.m_testType->currentData().toString(),QString("control"));QCOMPARE(window.m_target->currentText(),QString("192.0.2.25"));QCOMPARE(window.m_port->value(),11100);QVERIFY(window.m_probeGuide->text().contains("Sul DOCENTE"));QCOMPARE(window.m_linkAction->currentIndex(),0);QVERIFY(window.m_sendBox->isHidden());
        window.m_role->setCurrentIndex(2);QCOMPARE(window.m_testType->currentData().toString(),QString("discovery"));QCOMPARE(window.m_target->currentText(),QString("192.0.2.10"));QCOMPARE(window.m_port->value(),3702);QVERIFY(!window.m_protocol->isEnabled());QVERIFY(window.m_probeGuide->text().contains("UDP 3702"));QCOMPARE(window.m_linkAction->currentIndex(),1);QVERIFY(window.m_receiveBox->isHidden());QCOMPARE(window.m_linkRole->currentIndex(),2);
        window.m_testType->setCurrentIndex(0);QVERIFY(window.m_probeGuide->text().contains("ATTENZIONE"));window.m_testType->setCurrentIndex(2);QVERIFY(window.m_protocol->isEnabled());QVERIFY(!window.m_port->isReadOnly());
        QVERIFY(window.findChild<QLabel*>("companionLinkGuide")->text().contains("Torna sul DOCENTE"));QVERIFY(!window.m_stopLink->isEnabled());QVERIFY(!window.m_copyCode->isEnabled());window.close();
    }
    void discoveryProbeIsNotReportedAsAuthenticationAndIsSaved() {
        QTcpServer server;QVERIFY(server.listen(QHostAddress::LocalHost,0));CompanionWindow window(fixture(),false);window.m_role->setCurrentIndex(2);
        window.m_target->setEditText("127.0.0.1");window.m_port->setValue(server.serverPort());window.m_probe->click();QVERIFY(!window.m_testType->isEnabled());
        QTRY_VERIFY_WITH_TIMEOUT(window.m_probe->isEnabled(),3000);QVERIFY(window.m_probeResult->text().contains("Discovery diretto"));QVERIFY(window.m_probeResult->text().contains("multicast UDP NON verificati"));
        const auto tests=window.m_report["connectionTests"].toArray();QCOMPARE(tests.size(),1);const auto test=tests[0].toObject();QCOMPARE(test["kind"].toString(),QString("discovery"));QCOMPARE(test["target"].toString(),QString("127.0.0.1"));QCOMPARE(test["port"].toInt(),int(server.serverPort()));QVERIFY(test["tcpReachable"].toBool());QVERIFY(!test["authenticationVerified"].toBool());QVERIFY(!test["multicastVerified"].toBool());QVERIFY(!test["actualEndpoint"].toString().isEmpty());
        QVERIFY(!Companion::publicReport(window.m_report,false).contains("connectionTests"));window.close();
    }
    void trayAndManualServiceDiagnosticsAreExplicit() {
        auto report=fixture();report["identity"]=QJsonObject{{"sessionId",1}};report["processes"]=QJsonArray{QJsonObject{{"application","veyon-server.exe"},{"sessionId",1}}};
        report["services"]=QJsonArray{QJsonObject{{"name","VeyonSchoolService"},{"state","running"},{"startType",3}}};
        const auto advice=Companion::recommendations(report,"teacher").join('\n');QVERIFY(advice.contains("nessun processo worker"));QVERIFY(advice.contains("avvio manuale"));
        auto settings=report["settings"].toObject();settings["Service/HideTrayIcon"]=true;report["settings"]=settings;QVERIFY(Companion::recommendations(report,"teacher").join('\n').contains("nascosta dalla configurazione"));
        const auto filtered=Companion::publicReport(report,false);QVERIFY(!filtered.contains("processes"));
    }
    void renamedWorkerIsRecognizedAlongsideLegacyReports() {
        for(bool legacy:{false,true}) {
            auto report=fixture();report["identity"]=QJsonObject{{"sessionId",1}};
            const auto prefix=legacy?QString("veyon-"):QString("netlink-");
            report["processes"]=QJsonArray{QJsonObject{{"application",prefix+"server.exe"},{"sessionId",1}}};
            QVERIFY(Companion::recommendations(report,"teacher").join('\n').contains("nessun processo worker"));
            auto processes=report["processes"].toArray();processes.append(QJsonObject{{"application",prefix+"worker.exe"},{"sessionId",1}});report["processes"]=processes;
            QVERIFY(!Companion::recommendations(report,"teacher").join('\n').contains("nessun processo worker"));
        }
    }
    void serviceStartupFailuresAndDisabledAutostartAreExplicit() {
        auto report=fixture();auto settings=report["settings"].toObject();settings["Service/Autostart"]=true;report["settings"]=settings;
        report["services"]=QJsonArray{QJsonObject{{"name","VeyonSchoolService"},{"state","stopped"},{"startType",4},
            {"win32ExitCode",1068},{"serviceSpecificExitCode",0},{"recoveryPolicyVerified",false}}};
        const auto advice=Companion::recommendations(report,"student").join('\n');
        QVERIFY(advice.contains("avvio manuale o disabilitato"));QVERIFY(advice.contains("non applicato al servizio Windows"));
        QVERIFY(advice.contains("1068"));QVERIFY(advice.contains("registro Sistema"));QVERIFY(advice.contains("criteri di ripristino"));
        const auto before=QJsonDocument(report).toJson();Companion::recommendations(report,"teacher");QCOMPARE(QJsonDocument(report).toJson(),before);
    }
    void validTargets() {
        for(const auto& value:{"PC-DOCENTE","pc.aula.local","192.0.2.10","2001:db8::1","127.0.0.1"}) QVERIFY(Companion::validTarget(value));
        for(const auto& value:{"","0.0.0.0","::","224.0.0.1","255.255.255.255","999.2.3.4","http://pc","pc:11100","192.0.2.0/24"," pc","pc;whoami","a..b","-pc","pc-"}) QVERIFY(!Companion::validTarget(value));
    }
    void roundTripAndSchema() {
        QJsonObject result;QString error;const auto bytes=QJsonDocument(fixture()).toJson();QVERIFY(Companion::parseReport(bytes,result,error));QCOMPARE(result,fixture());
        for(const auto& invalid:{QByteArray("{}"),QByteArray("[]"),QByteArray("not json"),QByteArray(Companion::MaximumReportBytes+1,' ')}) {QVERIFY(!Companion::parseReport(invalid,result,error));QVERIFY(result.isEmpty());}
        auto wrong=fixture();wrong["schema"]=2;QVERIFY(!Companion::parseReport(QJsonDocument(wrong).toJson(),result,error));
    }
    void limits() {
        auto report=fixture();QJsonArray array;for(int i=0;i<129;++i) array.append(QJsonObject{{"name","x"}});report["keys"]=array;
        QJsonObject output;QString error;QVERIFY(!Companion::parseReport(QJsonDocument(report).toJson(),output,error));
        QJsonObject nested{{"x",true}};for(int i=0;i<15;++i) nested=QJsonObject{{"x",nested}};report=fixture();report["unknown"]=nested;QVERIFY(!Companion::parseReport(QJsonDocument(report).toJson(),output,error));
        QJsonArray manyAddresses;for(int i=0;i<257;++i) manyAddresses.append(QJsonObject{{"address","192.0.2.1"},{"prefix",24}});report=fixture();report["interfaces"]=QJsonArray{QJsonObject{{"addresses",manyAddresses}}};QVERIFY(!Companion::parseReport(QJsonDocument(report).toJson(),output,error));
    }
    void privacyProjection() {
        auto report=fixture();report["identity"]=QJsonObject{{"user","PRIVATE_USER"},{"sid","PRIVATE_SID"}};report["newField"]=QJsonObject{{"futureSecret","PRIVATE_FUTURE"}};
        const auto filtered=Companion::publicReport(report,false);QCOMPARE(filtered["identifiersIncluded"].toBool(),false);
        const auto bytes=QJsonDocument(filtered).toJson();for(const auto& value:{"PRIVATE_USER","PRIVATE_SID","PRIVATE_FUTURE","192.0.2.10","PC-DIMOSTRATIVO"}) QVERIFY(!bytes.contains(value));
        QCOMPARE(Companion::publicReport(report,true),report);QJsonObject reopened;QString error;QVERIFY(Companion::parseReport(bytes,reopened,error));
    }
    void staticComparison() {
        auto teacher=fixture();auto client=fixture("198.51.100.8");auto settings=client["settings"].toObject();settings["School/DiscoveryMaster"]="";client["settings"]=settings;
        const auto result=Companion::compare(teacher,client).join('\n');QVERIFY(result.contains("subnet IP comune"));QVERIFY(result.contains("DiscoveryMaster è vuoto"));QVERIFY(result.contains("file di chiave pubblica identico"));QVERIFY(result.contains("non firmati"));
        settings["Authentication/Method"]=0;client["settings"]=settings;QVERIFY(Companion::compare(teacher,client).join('\n').contains("metodi di autenticazione"));
        QVERIFY(Companion::compare(teacher,Companion::publicReport(client,false)).join('\n').contains("rapporti completi"));
    }
    void elevationAndPrivateKeyWarnings() {
        auto report=fixture();report["identity"]=QJsonObject{{"elevated",true}};auto keys=report["keys"].toArray();keys.append(QJsonObject{{"name","docente"},{"type","private"},{"readableByToolUser",true}});report["keys"]=keys;
        const auto warnings=Companion::recommendations(report,"client").join('\n');QVERIFY(warnings.contains("NON prova"));QVERIFY(warnings.contains("chiavi private"));
    }
    void htmlEscaping() {
        auto report=fixture();report["system"]=QJsonObject{{"computer","<script>bad()</script>"}};const auto output=Companion::html(report,"teacher");QVERIFY(!output.contains("<script>"));QVERIFY(output.contains("&lt;script&gt;"));
    }
    void copyrightOnlyAppearsInExportMetadata() {
        const auto report=fixture();
        QVERIFY(!Companion::html(report,"teacher").contains(NetLinkUi::copyrightText()));
        for(bool identifiers:{false,true}) {
            const auto exported=Companion::exportedReport(report,identifiers);
            QCOMPARE(exported["copyright"].toString().count(NetLinkUi::copyrightText()),1);
            QVERIFY(exported["copyright"].toString().contains("Veyon"));
            auto contents=exported;contents.remove("copyright");
            QCOMPARE(contents,Companion::publicReport(report,identifiers));
            QJsonObject reopened;QString error;
            QVERIFY2(Companion::parseReport(QJsonDocument(exported).toJson(),reopened,error),qPrintable(error));
            QCOMPARE(reopened,exported);
        }
        CompanionWindow window(report,false);QCOMPARE(window.windowTitle(),QString("NetLink Companion"));
        auto view=window.findChild<QTextBrowser*>("companionReport");QVERIFY(view);
        QVERIFY(!view->toPlainText().contains(NetLinkUi::copyrightText()));
        window.close();
    }
    void subnetMaskAsymmetry() {
        auto teacher=fixture("192.0.2.10");auto client=fixture("192.0.3.10");auto interfaces=client["interfaces"].toArray();auto iface=interfaces[0].toObject();iface["addresses"]=QJsonArray{QJsonObject{{"address","192.0.3.10"},{"prefix",16}}};interfaces[0]=iface;client["interfaces"]=interfaces;
        QVERIFY(Companion::compare(teacher,client).join('\n').contains("Non risulta una subnet IP comune"));
    }
    void tcpProbeLoopbackOnly() {
        QTcpServer server;QVERIFY(server.listen(QHostAddress::LocalHost,0));CompanionWindow window(fixture(),false);
        window.findChild<QCheckBox*>("companionProtocol")->setChecked(false);
        window.findChild<QComboBox*>("companionTarget")->setEditText("127.0.0.1");window.findChild<QSpinBox*>("companionPort")->setValue(server.serverPort());
        auto button=window.findChild<QPushButton*>("companionProbe");button->click();auto result=window.findChild<QLabel*>("companionProbeResult");
        QTRY_VERIFY_WITH_TIMEOUT(button->isEnabled(),3000);QVERIFY(result->text().contains("TCP raggiungibile"));QVERIFY(result->text().contains("NON verificata"));QCOMPARE(result->property("diagnosticState").toString(),QString("success"));QVERIFY(result->autoFillBackground());
        const auto output=qEnvironmentVariable("NETLINK_COMPANION_SCREENSHOTS");if(!output.isEmpty()){QDir().mkpath(output);window.show();QApplication::processEvents();QVERIFY(window.grab().save(QDir(output).filePath("netlink-companion-tcp-success.png")));}window.close();
    }
    void protocolProbeLoopback() {
        QTcpServer server;QVERIFY(server.listen(QHostAddress::LocalHost,0));
        connect(&server,&QTcpServer::newConnection,this,[&server]{auto socket=server.nextPendingConnection();socket->write("RFB 003.008\n");});
        CompanionWindow window(fixture(),false);window.findChild<QComboBox*>("companionTarget")->setEditText("127.0.0.1");window.findChild<QSpinBox*>("companionPort")->setValue(server.serverPort());
        auto button=window.findChild<QPushButton*>("companionProbe");button->click();auto result=window.findChild<QLabel*>("companionProbeResult");
        QTRY_VERIFY_WITH_TIMEOUT(button->isEnabled(),3000);QVERIFY(result->text().contains("banner RFB"));QVERIFY(result->text().contains("NON verificata"));QCOMPARE(result->property("diagnosticState").toString(),QString("success"));window.close();
    }
    void firewallAndListenerDiagnostics() {
        auto report=fixture();auto settings=report["settings"].toObject();settings["Network/FirewallExceptionEnabled"]=false;report["settings"]=settings;
        report["listeners"]=QJsonArray{QJsonObject{{"port",11100},{"address","127.0.0.1"}}};
        auto advice=Companion::recommendations(report,"client").join('\n');QVERIFY(advice.contains("eccezione firewall automatica disabilitata"));QVERIFY(advice.contains("solo su loopback"));
        report["listeners"]=QJsonArray{};QVERIFY(Companion::recommendations(report,"client").join('\n').contains("nessun listener"));
        auto system=report["system"].toObject();system["companionVersion"]="4.7.5-netlink.0.1.0-alpha.4";report["system"]=system;QVERIFY(Companion::recommendations(report,"teacher").join('\n').contains("lettore di autenticazione"));
        system["companionVersion"]="4.7.5-netlink.0.1.0-alpha.5";report["system"]=system;QVERIFY(!Companion::recommendations(report,"teacher").join('\n').contains("Questa alpha precede"));
    }
    void windowDestructionWithPendingReception() {
        auto window=new CompanionWindow(fixture(),false);
        QPointer<Companion::DiagnosticLink> link=window->findChild<Companion::DiagnosticLink*>();QVERIFY(link);
        QVERIFY(link->receiveOnce(QHostAddress::LocalHost));QVERIFY(!link->pairingCode().isEmpty());
        delete window;QVERIFY(link.isNull());QApplication::processEvents();
    }
    void widgetPreview() {
        QTemporaryDir directory;QVERIFY(directory.isValid());QSettings settings(directory.filePath("appearance.ini"),QSettings::IniFormat);
        auto appearance=NetLinkAppearance::install(&settings);CompanionWindow window(fixture(),false);window.show();QApplication::processEvents();
        QVERIFY(!window.windowIcon().isNull());auto view=window.findChild<QTextBrowser*>("companionReport");QVERIFY(view);QVERIFY(view->toPlainText().contains("PC-DIMOSTRATIVO"));
        auto role=window.findChild<QComboBox*>("companionRole");QVERIFY(role);role->setCurrentIndex(1);QVERIFY(view->toPlainText().contains("chiave privata leggibile"));
        const auto output=qEnvironmentVariable("NETLINK_COMPANION_SCREENSHOTS");if(!output.isEmpty()){QDir().mkpath(output);QVERIFY(window.grab().save(QDir(output).filePath("netlink-companion.png")));window.findChild<QTabWidget*>()->setCurrentIndex(1);QApplication::processEvents();QVERIFY(window.grab().save(QDir(output).filePath("netlink-companion-protected-link.png")));}
        window.close();delete appearance;
    }
};
QTEST_MAIN(CompanionTests)
#include "CompanionTests.moc"
