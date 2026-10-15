// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025-26 prof. Ing. Raffaele Mele
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include "HealthDiagnostics.h"
#include "Diagnostics.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QtTest>

class HealthDiagnosticsTests : public QObject
{
    Q_OBJECT
    QByteArray fixture(const QByteArray& app="dwm.exe",bool named=false) {
        QByteArray xml="<Event xmlns='http://schemas.microsoft.com/win/2004/08/events/event'><System><Provider Name='Application Error'/><EventID>1000</EventID><TimeCreated SystemTime='2026-10-15T17:00:00.000Z'/><Computer>PRIVATE_COMPUTER</Computer></System><EventData>";
        const QList<QByteArray> names{"AppName","AppVersion","AppTimeStamp","ModuleName","ModuleVersion","ModuleTimeStamp","ExceptionCode","FaultingOffset","ProcessId","ProcessCreationTime","AppPath","ModulePath"};
        const QList<QByteArray> values{app,"1","2","libwinpthread-1.dll","3","4","40000015","a38b","123","456","C:\\PRIVATE_USER\\app.exe","C:\\Program Files\\Veyon School\\libwinpthread-1.dll"};
        for(int i=0;i<values.size();++i) xml+="<Data"+(named?" Name='"+names[i]+"'":QByteArray{})+">"+values[i]+"</Data>";
        return xml+"</EventData></Event>";
    }
private Q_SLOTS:
    void legacyAndNamedWindowsEvents() {
        for(bool named:{false,true}) {
            const auto event=Companion::crashEvent(fixture("dwm.exe",named));
            QCOMPARE(event["kind"].toString(),QString("desktop-crash"));
            QCOMPARE(event["module"].toString(),QString("libwinpthread-1.dll"));
            QCOMPARE(event["exception"].toString(),QString("40000015"));
            QVERIFY(event["moduleFromSuitePath"].toBool());
            QVERIFY(!event["timeUtc"].toString().isEmpty());
            const auto bytes=QJsonDocument(event).toJson();
            QVERIFY(!bytes.contains("PRIVATE_USER"));QVERIFY(!bytes.contains("PRIVATE_COMPUTER"));QVERIFY(!bytes.contains("Program Files"));
        }
        auto precise=fixture();precise.replace("17:00:00.000Z","17:00:00.9560009Z");
        QVERIFY(!Companion::crashEvent(precise)["timeUtc"].toString().isEmpty());
    }
    void unrelatedAndMalformedEventsAreRejected() {
        QVERIFY(Companion::crashEvent(fixture("unrelated.exe")).isEmpty());
        auto xml=fixture();xml.replace("Application Error","Other Provider");QVERIFY(Companion::crashEvent(xml).isEmpty());
        xml=fixture();xml.replace("<EventID>1000","<EventID>1001");QVERIFY(Companion::crashEvent(xml).isEmpty());
        QVERIFY(Companion::crashEvent("<Event>").isEmpty());
        QVERIFY(Companion::crashEvent(QByteArray(128*1024+1,' ')).isEmpty());
        QVERIFY(Companion::crashEvent("<!DOCTYPE Event [<!ENTITY x 'bad'>]><Event>&x;</Event>").isEmpty());
    }
    void suiteApplicationCrashIsDistinctFromDesktopCrash() {
        const auto event=Companion::crashEvent(fixture("veyon-master.exe",true));
        QCOMPARE(event["kind"].toString(),QString("suite-crash"));
        QVERIFY(Companion::crashEvent(fixture("netlink-companion.exe")).contains("module"));
    }
    void recommendationsKeepEvidenceAndLimitsSeparate() {
        QJsonObject report{{"health",QJsonObject{{"eventLogAvailable",true},{"events",QJsonArray{Companion::crashEvent(fixture())}},
            {"applications",QJsonArray{QJsonObject{{"application","veyon-master.exe"},{"notResponding",true}}}}}}};
        const auto advice=Companion::recommendations(report,"teacher").join('\n');
        QVERIFY(advice.contains("arresti del desktop"));QVERIFY(advice.contains("precedere l'aggiornamento"));QVERIFY(advice.contains("non rispondente"));
        const auto output=Companion::html(report,"teacher");QVERIFY(output.contains("Stabilità e arresti Windows"));
        report["health"]=QJsonObject{{"eventLogAvailable",false}};
        QVERIFY(Companion::recommendations(report,"teacher").join('\n').contains("NON dimostra stabilità"));
    }
    void defaultExportOmitsHealthDetails() {
        QJsonObject report{{"health",QJsonObject{{"events",QJsonArray{Companion::crashEvent(fixture())}}}}};
        QVERIFY(!Companion::publicReport(report,false).contains("health"));
        QCOMPARE(Companion::publicReport(report,true),report);
    }
};
QTEST_GUILESS_MAIN(HealthDiagnosticsTests)
#include "HealthDiagnosticsTests.moc"
