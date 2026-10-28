// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include <QtTest>
#include <QSettings>
#include <QTemporaryDir>
#include <QFile>
#include <QProcess>
#include <QDataStream>
#include <QCryptographicHash>
#include "Configuration/LocalStore.h"
#include "Configuration/Object.h"
#include "Configuration/SettingsTransaction.h"
#include "DiscoveryImportController.h"

class TestStore : public Configuration::LocalStore {
public:
    QString file;explicit TestStore(const QString& path):LocalStore(Configuration::Store::System),file(path){}
    QSettings* createSettingsObject() const override {return new QSettings(file,QSettings::IniFormat);}
};
static QByteArray journal(const Configuration::SettingsTransaction::Values& before,const Configuration::SettingsTransaction::Values& after) {
    QByteArray data;QDataStream stream(&data,QIODevice::WriteOnly);stream.setVersion(QDataStream::Qt_5_12);stream<<before<<after;
    return QByteArray("NLT1",4)+QCryptographicHash::hash(data,QCryptographicHash::Sha256)+data;
}
class ConfigurationRepositoryTests : public QObject {
    Q_OBJECT
    QTemporaryDir temp;
private Q_SLOTS:
    void disjointConfiguratorAndDiscoveryEditsAreMergedWithoutChangingKeys() {
        TestStore store(temp.filePath("merge.ini"));QString error;
        QVERIFY(store.writeValue("NetworkObjects",QJsonArray{"original"},"BuiltinDirectory"));
        QVERIFY(store.writeValue("PrivateKeyBaseDir","inert-private-path","Authentication"));
        QVERIFY(store.writeValue("PublicKeyBaseDir","inert-public-path","Authentication"));
        QVERIFY(store.writeValue("AuthenticationKey","key_a","Master"));
        Configuration::Object configurator(&store);
        QVERIFY(store.compareAndWriteValue("NetworkObjects",QJsonArray{"original"},QJsonArray{"original","imported"},"BuiltinDirectory",error));
        configurator.setValue("Language","it_IT","UI");QVERIFY2(store.save(configurator,error),qPrintable(error));
        Configuration::Object loaded(&store);
        QCOMPARE(loaded.value("NetworkObjects","BuiltinDirectory",{}).toJsonArray(),QJsonArray({"original","imported"}));
        QCOMPARE(configurator.data(),loaded.data());QCOMPARE(loaded.value("AuthenticationKey","Master",{}).toString(),QString("key_a"));
        QCOMPARE(loaded.value("PrivateKeyBaseDir","Authentication",{}).toString(),QString("inert-private-path"));
        QCOMPARE(loaded.value("PublicKeyBaseDir","Authentication",{}).toString(),QString("inert-public-path"));
        QVERIFY(!loaded.data().contains("_NetLinkTransaction"));
        configurator.setValue("Language","en_US","UI");QVERIFY(store.save(configurator,error));
    }
    void overlappingEditsAreRejectedWithoutWritingAnyField() {
        TestStore store(temp.filePath("conflict.ini"));QString error;QVERIFY(store.writeValue("Value","base"));
        Configuration::Object a(&store),b(&store);a.setValue("Value","a",{});b.setValue("Value","b",{});b.setValue("Extra","must_not_save",{});
        QVERIFY(store.save(a,error));QVERIFY(!store.save(b,error));QVERIFY(error.contains("Value"));
        Configuration::Object after(&store);QCOMPARE(after.value("Value",{},{}).toString(),QString("a"));QVERIFY(!after.hasValue("Extra",{}));
        QVERIFY(!store.compareAndWriteValue("Value","base","overwritten",{},error));
        QCOMPARE(after.value("Value",{},{}).toString(),QString("a"));
    }
    void failedWriteNeverClaimsSuccessOrClearsExistingData() {
        TestStore store(temp.filePath("failure.ini"));QString error;QVERIFY(store.writeValue("Value","preserve"));
        Configuration::Object staged(&store);staged.setValue("Value","new",{});
        QFile obstruction(temp.filePath("obstruction"));QVERIFY(obstruction.open(QIODevice::WriteOnly));obstruction.close();
        TestStore unavailable(obstruction.fileName()+"/settings.ini");Configuration::Object initial;
        initial.setValue("Anything","new",{});QVERIFY(!unavailable.save(initial,error));QVERIFY(!error.isEmpty());
        Configuration::Object after(&store);QCOMPARE(after.value("Value",{},{}).toString(),QString("preserve"));
        // A baseline from a different store must not authorize replacement.
        QVERIFY(!unavailable.save(staged,error));
    }
    void interruptedWritesRecoverBeforeExposingSettings_data() {
        QTest::addColumn<QString>("stage");for(const auto name:{"before","partial","complete","corrupt"}) QTest::newRow(name)<<QString(name);
    }
    void interruptedWritesRecoverBeforeExposingSettings() {
        QFETCH(QString,stage);const auto path=temp.filePath(stage+".ini");QSettings settings(path,QSettings::IniFormat);
        const Configuration::SettingsTransaction::Values before{{"A","old_a"},{"B","old_b"}},after{{"A","new_a"},{"B","new_b"}};
        for(auto it=before.cbegin();it!=before.cend();++it) settings.setValue(it.key(),it.value());
        settings.setValue("_NetLinkTransaction/RecoveryV1",stage=="corrupt"?QByteArray("invalid"):journal(before,after));
        if(stage=="partial" || stage=="complete") settings.setValue("A","new_a");
        if(stage=="complete") settings.setValue("B","new_b");
        settings.sync();
        TestStore store(path);Configuration::Object loaded(&store);
        if(stage=="corrupt") {QVERIFY(!loaded.property("netlinkConfigurationReadError").toString().isEmpty());QVERIFY(loaded.data().isEmpty());return;}
        QVERIFY2(store.errorString().isEmpty(),qPrintable(store.errorString()));
        QCOMPARE(loaded.value("A",{},{}).toString(),stage=="complete"?QString("new_a"):QString("old_a"));
        QCOMPARE(loaded.value("B",{},{}).toString(),stage=="complete"?QString("new_b"):QString("old_b"));
        QString error;loaded.setValue("C","saved",{});QVERIFY(store.save(loaded,error));
        QSettings checked(path,QSettings::IniFormat);QVERIFY(!checked.contains("_NetLinkTransaction/RecoveryV1"));
    }
    void clearAndUnsetAreCheckedAndDoNotResurrectRemovedValues() {
        TestStore store(temp.filePath("clear.ini"));QString error;QVERIFY(store.writeValue("Keep","yes","Group"));QVERIFY(store.writeValue("Remove","yes","Group"));
        Configuration::Object object(&store);object.removeValue("Remove","Group");QVERIFY(store.save(object,error));
        object.reloadFromStore();QVERIFY(!object.hasValue("Remove","Group"));QVERIFY(object.hasValue("Keep","Group"));
        QVERIFY(store.clearChecked(error));object.reloadFromStore();QVERIFY(object.data().isEmpty());
    }
    void preferenceFlushAdvancesBaselineAndCanRemoveNewValues() {
        TestStore store(temp.filePath("preferences.ini"));Configuration::Object object(&store);
        object.setValue("Temporary","value","Preferences");object.flushStore();QVERIFY(object.property("netlinkConfigurationWriteError").toString().isEmpty());
        object.removeValue("Temporary","Preferences");object.flushStore();
        Configuration::Object loaded(&store);QVERIFY(!loaded.hasValue("Temporary","Preferences"));
    }
    void twoProcessesCannotBothCommitTheSameExpectedRevision() {
        const auto directory=temp.filePath("processes");QVERIFY(QDir().mkpath(directory));QFile marker(directory+"/.audit-fixture");QVERIFY(marker.open(QIODevice::WriteOnly));marker.close();
        TestStore store(directory+"/shared.ini");QVERIFY(store.writeValue("Value","base"));
        QProcess a,b;const auto exe=QCoreApplication::applicationFilePath();
        a.start(exe,{"--writer",directory,"a"});b.start(exe,{"--writer",directory,"b"});
        QVERIFY(a.waitForStarted(5000));QVERIFY(b.waitForStarted(5000));
        QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(directory+"/ready-a") && QFile::exists(directory+"/ready-b"),5000);
        QFile go(directory+"/go");QVERIFY(go.open(QIODevice::WriteOnly));go.close();
        QVERIFY(a.waitForFinished(10000));QVERIFY(b.waitForFinished(10000));
        QCOMPARE(int(a.exitCode()==0)+int(b.exitCode()==0),1);
        Configuration::Object after(&store);QVERIFY(after.value("Value",{},{}).toString()=="a" || after.value("Value",{},{}).toString()=="b");
    }
    void processCrashRecoversPartialWrite() {
        const auto directory=temp.filePath("crash");QVERIFY(QDir().mkpath(directory));QFile marker(directory+"/.audit-fixture");QVERIFY(marker.open(QIODevice::WriteOnly));marker.close();
        TestStore store(directory+"/shared.ini");QVERIFY(store.writeValue("A","old_a"));QVERIFY(store.writeValue("B","old_b"));
        QProcess process;process.start(QCoreApplication::applicationFilePath(),{"--interrupt",directory,"fixture"});QVERIFY(process.waitForFinished(10000));QCOMPARE(process.exitCode(),19);
        Configuration::Object loaded(&store);QVERIFY2(store.errorString().isEmpty(),qPrintable(store.errorString()));
        QCOMPARE(loaded.value("A",{},{}).toString(),QString("old_a"));QCOMPARE(loaded.value("B",{},{}).toString(),QString("old_b"));
    }
#ifdef Q_OS_WIN
    void nativeRegistryRecoveryIsProtectedAndRetainsOldValues() {
        const auto name="fixture-"+QUuid::createUuid().toString(QUuid::WithoutBraces);
        class NativeStore : public Configuration::LocalStore {
            QString name;
        public:
            explicit NativeStore(const QString& app):LocalStore(Configuration::Store::User),name(app){}
            QSettings* createSettingsObject() const override {return new QSettings(QSettings::NativeFormat,QSettings::UserScope,"NetLinkConfigurationTests",name);}
        } store(name);
        QSettings settings(QSettings::NativeFormat,QSettings::UserScope,"NetLinkConfigurationTests",name);
        const Configuration::SettingsTransaction::Values before{{"A","old_a"},{"B","old_b"}},after{{"A","new_a"},{"B","new_b"}};
        const auto protectedRecord=Configuration::SettingsTransaction::recoveryRecord(settings,before,after);QVERIFY(protectedRecord.startsWith("NLE1"));
        settings.setValue("A","new_a");settings.setValue("B","old_b");settings.setValue("_NetLinkTransaction/RecoveryV1",protectedRecord);settings.sync();
        Configuration::Object loaded(&store);const auto error=store.errorString();
        const auto a=loaded.value("A",{},{}).toString(),b=loaded.value("B",{},{}).toString();
        settings.clear();settings.sync(); // Only this randomly named HKCU test subtree.
        QVERIFY2(error.isEmpty(),qPrintable(error));QCOMPARE(a,QString("old_a"));QCOMPARE(b,QString("old_b"));
    }
#endif
    void pureImportPlanRejectsLatePortDecoratedDuplicateAndIsAtomic() {
        const auto uid=QUuid::createUuid();NetworkObject group(NetworkObject::Type::Location,"Room",{},{},{},uid);
        QJsonArray before{group.toJson(),NetworkObject(NetworkObject::Type::Host,"Existing","192.0.2.25:11100",{},{},QUuid::createUuid(),uid).toJson()};
        SchoolDiscovery::Record record;record.endpoint="urn:uuid:"+QUuid::createUuid().toString(QUuid::WithoutBraces);record.name="Candidate";record.address="192.0.2.25";
        QJsonArray after;QString error;QVERIFY(!DiscoveryImportController::append(before,{group,{record}},after,error));QCOMPARE(after,before);
    }
};
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);const auto arguments=app.arguments();
    if(arguments.size()==4 && arguments[1]=="--interrupt") {
        const auto directory=arguments[2];if(!QFile::exists(directory+"/.audit-fixture") || arguments[3]!="fixture") return 9;
        QSettings settings(directory+"/shared.ini",QSettings::IniFormat);Configuration::SettingsLock lock(settings);if(!lock.locked()) return 8;
        const Configuration::SettingsTransaction::Values before{{"A","old_a"},{"B","old_b"}},after{{"A","new_a"},{"B","new_b"}};
        settings.setValue("_NetLinkTransaction/RecoveryV1",journal(before,after));settings.setValue("A","new_a");settings.sync();std::_Exit(19);
    }
    if(arguments.size()==4 && arguments[1]=="--writer") {
        const auto directory=arguments[2];if(!QFile::exists(directory+"/.audit-fixture") || (arguments[3]!="a" && arguments[3]!="b")) return 9;
        QFile ready(directory+"/ready-"+arguments[3]);if(!ready.open(QIODevice::WriteOnly)) return 8;ready.close();
        QElapsedTimer deadline;deadline.start();while(!QFile::exists(directory+"/go")) {if(deadline.elapsed()>5000) return 7;QThread::msleep(5);}
        TestStore store(directory+"/shared.ini");QString error;return store.compareAndWriteValue("Value","base",arguments[3],{},error)?0:3;
    }
    ConfigurationRepositoryTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "ConfigurationRepositoryTests.moc"
