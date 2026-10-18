// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include <QtTest>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTemporaryDir>
#include "ClassProfiles.h"
#include "NetLinkUi.h"
#include "SchoolFeatures.h"
#include "VeyonMasterInterface.h"

class ProfileMaster : public VeyonMasterInterface {
public:
    ProfileMaster() : VeyonMasterInterface(nullptr),local(Computer()) {}
    QWidget* mainWindow() override {return &window;}
    Configuration::Object* userConfigurationObject() override {return nullptr;}
    void reloadSubFeatures() override {}
    ComputerControlInterface& localSessionControlInterface() override {return local;}
    const ComputerControlInterfaceList& allComputerControlInterfaces() const override {return clients;}
    ComputerControlInterfaceList selectedComputerControlInterfaces() const override {return selected;}
    ComputerControlInterfaceList filteredComputerControlInterfaces() const override {return clients;}
    QWidget window;ComputerControlInterface local;ComputerControlInterfaceList clients,selected;
};
class ClassProfileTests : public QObject {
    Q_OBJECT
    QTemporaryDir directory;
    SchoolClassProfile sample() const {return {"3A","Laboratorio 1",{{QUuid("c1d6fba0-63db-465a-8314-51d846a52fd5"),"PC-01","192.0.2.4","Anna Rossi"}}};}
    static ComputerControlInterface::Pointer client(const QUuid& id,const QString& host="192.0.2.4") {return ComputerControlInterface::Pointer::create(Computer(id,"PC-01",host,{},"Laboratorio 1"));}
    static bool acceptPath(QDialog* dialog,const QString& path) {
        auto chooser=dialog->findChild<QFileDialog*>("profileFileDialog");if(!chooser) return false;
        chooser->setDirectory(QFileInfo(path).absolutePath());
        QTest::qWait(150);
        auto filename=chooser->findChild<QLineEdit*>("fileNameEdit");if(!filename) return false;
        filename->setText(QFileInfo(path).fileName());
        QSignalSpy selected(chooser,&QFileDialog::fileSelected);
        const bool invoked=QMetaObject::invokeMethod(chooser,"accept",Qt::DirectConnection);
        if(selected.isEmpty()) qWarning()<<"File chooser did not accept"<<chooser->selectedFiles()<<chooser->isVisible();
        return invoked && selected.count()==1;
    }
private Q_SLOTS:
    void initTestCase() {QVERIFY(directory.isValid());QLocale::setDefault(QLocale("en_US"));}
    void jsonRoundtripContainsAssignmentsOnly() {
        auto profile=sample();profile.assignments[0].student="Éléonore 学生";
        SchoolClassProfile read;QString error;QVERIFY2(SchoolClassProfile::fromJson(profile.toJson(),read,&error),qPrintable(error));
        QCOMPARE(read.className,profile.className);QCOMPARE(read.room,profile.room);QCOMPARE(read.assignments[0].student,profile.assignments[0].student);
        const auto object=QJsonDocument::fromJson(profile.toJson()).object();QCOMPARE(object.size(),5);
        QVERIFY(!profile.toJson().contains("presence"));QVERIFY(!profile.toJson().contains("declaration"));
        QVERIFY(SchoolClassProfile::defaultDirectory().replace('\\','/').endsWith("NetLink/Classi"));
    }
    void invalidJsonNeverReplacesDestination_data() {
        QTest::addColumn<QByteArray>("bytes");auto root=QJsonDocument::fromJson(sample().toJson()).object();
        QTest::newRow("broken")<<QByteArray("{broken");QTest::newRow("array")<<QByteArray("[]");
        auto invalid=root;invalid["version"]=2;QTest::newRow("future-version")<<QJsonDocument(invalid).toJson();
        invalid=root;invalid["version"]=1.5;QTest::newRow("fraction")<<QJsonDocument(invalid).toJson();
        invalid=root;invalid["class"]=true;QTest::newRow("wrong-type")<<QJsonDocument(invalid).toJson();
        invalid=root;invalid["class"]=QString(81,'a');QTest::newRow("long-class")<<QJsonDocument(invalid).toJson();
        invalid=root;invalid["room"]=" ";QTest::newRow("blank-room")<<QJsonDocument(invalid).toJson();
        invalid=root;invalid["confirmed"]=true;QTest::newRow("unknown-field")<<QJsonDocument(invalid).toJson();
        auto seats=root["assignments"].toArray();seats.append(seats[0]);invalid=root;invalid["assignments"]=seats;QTest::newRow("duplicate")<<QJsonDocument(invalid).toJson();
        for(const auto& bad:QStringList{"", "not-a-uuid","00000000-0000-0000-0000-000000000000"}) {
            auto seat=root["assignments"].toArray()[0].toObject();seat["computerId"]=bad;invalid=root;invalid["assignments"]=QJsonArray{seat};QTest::newRow(qPrintable("id-"+bad))<<QJsonDocument(invalid).toJson();
        }
        for(const auto& bad:QStringList{QString(121,'a'),QString("Anna\nRossi"),QString("Anna")+QChar(0x202e)}) {
            auto seat=root["assignments"].toArray()[0].toObject();seat["student"]=bad;invalid=root;invalid["assignments"]=QJsonArray{seat};QTest::newRow(qPrintable("name-"+QString::number(bad.size())))<<QJsonDocument(invalid).toJson();
        }
        invalid=root;invalid["assignments"]=QJsonArray{};QTest::newRow("empty")<<QJsonDocument(invalid).toJson();
        seats=QJsonArray{};for(int i=0;i<257;++i) seats.append(root["assignments"].toArray()[0]);invalid=root;invalid["assignments"]=seats;QTest::newRow("too-many")<<QJsonDocument(invalid).toJson();
        QTest::newRow("oversized")<<QByteArray(SchoolClassProfile::MaximumBytes+1,' ');
    }
    void invalidJsonNeverReplacesDestination() {
        QFETCH(QByteArray,bytes);auto existing=sample();existing.className="Keep me";QString error;
        QVERIFY(!SchoolClassProfile::fromJson(bytes,existing,&error));QVERIFY(!error.isEmpty());QCOMPARE(existing.className,QString("Keep me"));
    }
    void atomicSaveAndReadErrors() {
        auto profile=sample();const auto path=directory.filePath("atomic.json");QString error;
        QVERIFY2(profile.save(path,&error),qPrintable(error));SchoolClassProfile read;QVERIFY(SchoolClassProfile::load(path,read,&error));QCOMPARE(read.toJson(),profile.toJson());
        QFile before(path);QVERIFY(before.open(QIODevice::ReadOnly));const auto bytes=before.readAll();before.close();
        profile.className.clear();QVERIFY(!profile.save(path,&error));QFile after(path);QVERIFY(after.open(QIODevice::ReadOnly));QCOMPARE(after.readAll(),bytes);
        QVERIFY(!SchoolClassProfile::load(directory.filePath("missing.json"),read,&error));QCOMPARE(read.className,QString("3A"));
        QVERIFY(!sample().save(directory.filePath("missing-dir/file.json"),&error));
    }
    void identifiersSurviveDhcpButNeverFollowAnIpOrName() {
        ProfileMaster master;const auto profile=sample();const auto id=profile.assignments[0].computerId;
        auto old=client(id);master.clients={old};ClassProfiles profiles(master);profiles.activate(profile);QCOMPARE(old->attendanceName(),QString("Anna Rossi"));QVERIFY(old->studentName().isEmpty());
        auto renamed=client(id,"192.0.2.99"),impostor=client(QUuid::createUuid(),"192.0.2.4");master.clients={renamed,impostor};profiles.synchronize();
        QCOMPARE(renamed->expectedStudentName(),QString("Anna Rossi"));QVERIFY(impostor->attendanceName().isEmpty());QVERIFY(old->attendanceName().isEmpty());
    }
    void startsFromDisplayedComputersButDirectLoadHasNoSpuriousDraft() {
        ProfileMaster master;auto ci=client(sample().assignments[0].computerId);master.clients={ci};
        ClassProfiles profiles(master);profiles.show();QCOMPARE(profiles.profile().assignments.size(),1);
        profiles.m_dialog->reject();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        ClassProfiles direct(master);direct.show("load");QTRY_VERIFY(direct.m_dialog->findChild<QFileDialog*>("profileFileDialog"));
        QVERIFY(direct.profile().assignments.isEmpty());QVERIFY(!direct.m_dirty);QVERIFY(!direct.m_dialog->findChild<QMessageBox*>());
    }
    void profileChangesRejectLateRepliesAndDoNotInventAttendance() {
        ProfileMaster master;auto a=sample();auto ci=client(a.assignments[0].computerId);master.clients={ci};ClassProfiles profiles(master);profiles.activate(a);
        SchoolFeatures features(nullptr,false);const auto nonce=QUuid::createUuid().toString(QUuid::WithoutBraces);ci->setProperty("schoolAttendanceRequest",nonce);
        auto b=a;b.className="4B";b.assignments[0].student="Luca Bianchi";profiles.activate(b);
        QVERIFY(features.handleFeatureMessage(ci,FeatureMessage(features.m_attendanceUid,SchoolFeatures::NameReply).addArgument(0,nonce).addArgument(1,"Anna Rossi")));
        QCOMPARE(ci->attendanceName(),QString("Luca Bianchi"));QVERIFY(ci->studentName().isEmpty());QCOMPARE(ci->attendanceStatus(),QString("Preloaded name"));
        ci->setStudentName("Nome diverso");QVERIFY(ci->attendanceStatus().contains("different"));ci->stop();QVERIFY(ci->studentName().isEmpty());QCOMPARE(ci->attendanceName(),QString("Luca Bianchi"));
        ci->setStudentName("Luca Bianchi");profiles.activate(b);QVERIFY(ci->studentName().isEmpty());
    }
    void rerequestOnlySelectedComputersKeepsOtherAssignments() {
        ProfileMaster master;const auto id=QUuid::createUuid();auto a=client(id),b=client(QUuid::createUuid());master.clients={a,b};master.selected={a};
        a->m_state=b->m_state=ComputerControlInterface::State::Connected;a->setExpectedStudentName("Anna");b->setExpectedStudentName("Luca");a->setStudentName("Old Anna");b->setStudentName("Old Luca");
        SchoolFeatures features(nullptr,false);QVERIFY(features.startFeature(master,features.featureList()[1],master.selected));
        auto box=qobject_cast<QMessageBox*>(features.m_attendanceDialog.data());QVERIFY(box);
        for(auto button:box->buttons()) if(button->text()=="Request names") button->click();
        QVERIFY(!a->property("schoolAttendanceRequest").toString().isEmpty());QVERIFY(a->studentName().isEmpty());QCOMPARE(a->expectedStudentName(),QString("Anna"));QCOMPARE(b->studentName(),QString("Old Luca"));
    }
    void editingAndFileDialogsRemainNonModal() {
        ProfileMaster master;const auto profile=sample();auto ci=client(profile.assignments[0].computerId);master.clients={ci};master.selected={ci};
        ClassProfiles profiles(master);profiles.activate(profile);profiles.show();auto dialog=profiles.m_dialog;QVERIFY(dialog);QCOMPARE(dialog->windowModality(),Qt::NonModal);
        auto table=dialog->findChild<QTableWidget*>("profileAssignments");QVERIFY(table);QCOMPARE(table->rowCount(),1);
        QVERIFY(dialog->windowTitle().endsWith(NetLinkUi::copyrightText()));
        table->item(0,1)->setText("Nome modificato");QCOMPARE(ci->expectedStudentName(),QString("Nome modificato"));QVERIFY(profiles.m_dirty);
        dialog->reject();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QVERIFY(!profiles.m_dialog);QCOMPARE(profiles.profile().assignments[0].student,QString("Nome modificato"));
        profiles.show("save");QTRY_VERIFY(profiles.m_dialog->findChild<QFileDialog*>("profileFileDialog"));const auto path=directory.filePath("ui-saved.json");QVERIFY(acceptPath(profiles.m_dialog,path));
        QTRY_VERIFY_WITH_TIMEOUT(!profiles.m_dirty,4000);SchoolClassProfile read;QVERIFY(SchoolClassProfile::load(path,read));QCOMPARE(read.assignments[0].student,QString("Nome modificato"));
        dialog=profiles.m_dialog;profiles.show("csv");QVERIFY(dialog->findChild<QFileDialog*>("profileFileDialog"));QVERIFY(acceptPath(dialog,directory.filePath("profile.csv")));QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(directory.filePath("profile.csv")),4000);
    }
    void invalidLoadDoesNotClearActiveNames() {
        ProfileMaster master;const auto profile=sample();auto ci=client(profile.assignments[0].computerId);master.clients={ci};ClassProfiles profiles(master);profiles.activate(profile);profiles.show("load");
        const auto path=directory.filePath("bad.json");QFile bad(path);QVERIFY(bad.open(QIODevice::WriteOnly));bad.write("{}");bad.close();
        QTRY_VERIFY(profiles.m_dialog->findChild<QFileDialog*>("profileFileDialog"));QVERIFY(acceptPath(profiles.m_dialog,path));QTRY_VERIFY_WITH_TIMEOUT(profiles.m_dialog->findChild<QMessageBox*>(),4000);
        QCOMPARE(ci->expectedStudentName(),QString("Anna Rossi"));QCOMPARE(profiles.profile().className,QString("3A"));
    }
    void successfulLoadAndClosedReadCannotOverwriteNewClass() {
        ProfileMaster master;auto a=sample(),b=a;b.className="4B";b.assignments[0].student="Luca Bianchi";
        auto ci=client(a.assignments[0].computerId);master.clients={ci};ClassProfiles profiles(master);profiles.activate(a);
        const auto path=directory.filePath("class-b.json");QVERIFY(b.save(path));profiles.show("load");
        QTRY_VERIFY(profiles.m_dialog->findChild<QFileDialog*>("profileFileDialog"));QVERIFY(acceptPath(profiles.m_dialog,path));
        QTRY_COMPARE_WITH_TIMEOUT(profiles.profile().className,QString("4B"),4000);QCOMPARE(ci->expectedStudentName(),QString("Luca Bianchi"));
        QTRY_VERIFY(!profiles.m_dialog->findChild<QFileDialog*>("profileFileDialog"));
        profiles.show("load");QVERIFY(acceptPath(profiles.m_dialog,path));profiles.m_dialog->reject();profiles.activate(a);
        QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QTest::qWait(250);
        QCOMPARE(profiles.profile().className,QString("3A"));QCOMPARE(ci->expectedStudentName(),QString("Anna Rossi"));
    }
    void csvEscapesFormulasAndKeepsDeclaredAndAssignedSeparate() {
        ProfileMaster master;auto profile=sample();profile.assignments[0].student="=1+2";auto ci=client(profile.assignments[0].computerId);master.clients={ci};
        ClassProfiles profiles(master);profiles.activate(profile);ci->setStudentName("Anna; \"Rossi\"");profiles.show("csv");
        QTRY_VERIFY(profiles.m_dialog->findChild<QFileDialog*>("profileFileDialog"));const auto path=directory.filePath("escaped.csv");QVERIFY(acceptPath(profiles.m_dialog,path));
        QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(path),4000);QFile csv(path);QVERIFY(csv.open(QIODevice::ReadOnly));const auto bytes=csv.readAll();
        QVERIFY(bytes.startsWith("\xef\xbb\xbf"));QVERIFY(bytes.contains("\"'=1+2\""));QVERIFY(bytes.contains("\"Anna; \"\"Rossi\"\"\""));QVERIFY(bytes.contains("different from assignment"));
        QCOMPARE(profiles.profile().assignments[0].student,QString("=1+2"));
    }
    void translatedEditorHasNoPresenceConfirmation() {
        struct Restore {QLocale locale;~Restore(){QLocale::setDefault(locale);}} restore{QLocale()};QLocale::setDefault(QLocale("it_IT"));
        ProfileMaster master;ClassProfiles profiles(master);profiles.activate(sample());profiles.show();auto dialog=profiles.m_dialog;
        QTRY_VERIFY(dialog->isVisible());QCOMPARE(dialog->findChild<QPushButton*>("profileSave")->text(),QString("Salva JSON…"));
        for(const auto button:dialog->findChildren<QPushButton*>()) QVERIFY(!button->text().contains("conferma",Qt::CaseInsensitive));
        auto table=dialog->findChild<QTableWidget*>("profileAssignments");QVERIFY(table->item(0,1)->flags().testFlag(Qt::ItemIsEditable));
        QVERIFY(!table->item(0,0)->flags().testFlag(Qt::ItemIsEditable));
        const auto screenshot=qEnvironmentVariable("NETLINK_PROFILE_SCREENSHOT");if(!screenshot.isEmpty()) QVERIFY(dialog->grab().save(screenshot));
    }
};
QTEST_MAIN(ClassProfileTests)
#include "ClassProfileTests.moc"
