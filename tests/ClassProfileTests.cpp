// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include <QtTest>
#include <QFileDialog>
#include <QFontDatabase>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QListView>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QSettings>
#include <QStandardItemModel>
#include <QStyleFactory>
#include <QTableWidget>
#include <QTreeView>
#include <QTemporaryDir>
#include <memory>
#include "ClassProfiles.h"
#include "ClassProfileDirectory.h"
#include "ComputerCaptionDelegate.h"
#include "NetLinkAppearance.h"
#include "NetLinkUi.h"
#include "SchoolFeatures.h"
#include "VeyonMasterInterface.h"
#include "SelectedFeatureControl.h"

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
    struct DirectoryFixture {
        QStandardItemModel model;
        CheckableItemProxyModel checked{NetworkObjectModel::UidRole};
        QSortFilterProxyModel search;
        QTreeView view;
        QUuid roomA=QUuid::createUuid(),roomB=QUuid::createUuid();
        QUuid a=QUuid::createUuid(),b=QUuid::createUuid(),c=QUuid::createUuid();
        QStandardItem* item(const QString& name,const QUuid& id,NetworkObject::Type type,const QString& host={}) {
            auto result=new QStandardItem(name);result->setData(id,NetworkObjectModel::UidRole);
            result->setData(QVariant::fromValue(type),NetworkObjectModel::TypeRole);
            result->setData(host,NetworkObjectModel::HostAddressRole);return result;
        }
        DirectoryFixture() {
            auto first=item("Room A",roomA,NetworkObject::Type::Location);
            first->appendRow(item("PC-01",a,NetworkObject::Type::Host,"192.0.2.1"));
            first->appendRow(item("PC-02",b,NetworkObject::Type::Host,"192.0.2.2"));
            auto second=item("Room B",roomB,NetworkObject::Type::Location);
            second->appendRow(item("PC-01",c,NetworkObject::Type::Host,"192.0.2.1"));
            model.appendRow(first);model.appendRow(second);checked.setSourceModel(&model);
            search.setSourceModel(&checked);search.setRecursiveFilteringEnabled(true);
            view.setModel(&search);view.setSelectionBehavior(QAbstractItemView::SelectRows);
            view.setCurrentIndex(search.index(0,0,search.index(0,0)));
            checked.setCheckedItems({a,c});
        }
        QUuid group() const {
            const auto selection=view.selectionModel();
            return selection->hasSelection()?ClassProfileDirectory::highlightedGroup(selection->currentIndex()):QUuid{};
        }
        void bind(ClassProfiles& profiles,std::function<bool()> canChange=[] {return true;}) {
            profiles.setDirectoryContext(&model,&checked,[this] {return group();},canChange);
        }
    };
private Q_SLOTS:
    void initTestCase() {
        QVERIFY(directory.isValid());QLocale::setDefault(QLocale("en_US"));
#ifdef Q_OS_WIN
        // Offscreen tests cannot always enumerate Windows' system font store.
        // Load the font file explicitly so pixel assertions check actual glyphs.
        QVERIFY(QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR","C:/Windows")+"/Fonts/segoeui.ttf")>=0);
        QVERIFY(QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR","C:/Windows")+"/Fonts/seguisb.ttf")>=0);
#endif
    }
    void thumbnailCaptionKeepsLoginComputerAndPolicy() {
        auto ci=client(QUuid::createUuid());ci->m_state=ComputerControlInterface::State::Connected;
        ci->setUserInformation("SCUOLA\\studente","Account condiviso",1);ci->setStudentName("Anna Rossi");
        for(auto content:{ComputerListModel::DisplayRoleContent::UserAndComputerName,
                          ComputerListModel::DisplayRoleContent::ComputerName,ComputerListModel::DisplayRoleContent::UserName})
            QCOMPARE(ComputerCaptionDelegate::captionText(*ci,content),QString("[Anna Rossi] - SCUOLA\\studente - PC-01"));
        ci->setNetworkPolicyStatus("blocked");
        ci->setProperty("schoolPolicyVerifiedState","blocked");
        const auto text=ComputerCaptionDelegate::captionText(*ci,ComputerListModel::DisplayRoleContent::UserAndComputerName);
        QCOMPARE(text,QString("[Anna Rossi] - SCUOLA\\studente - PC-01 [web locked]"));
        QVERIFY(!text.contains("Declared"));QVERIFY(!text.contains("Account condiviso"));
        ci->setExpectedStudentName("Luca Bianchi");ci->clearStudentDeclaration();
        QCOMPARE(ComputerCaptionDelegate::captionText(*ci,ComputerListModel::DisplayRoleContent::UserAndComputerName),
                 QString("[Luca Bianchi] - SCUOLA\\studente - PC-01 [web locked]"));
        QCOMPARE(ci->attendanceStatus(),QString("Preloaded name"));
        ci->setExpectedStudentName({});
        QCOMPARE(ComputerCaptionDelegate::captionText(*ci,ComputerListModel::DisplayRoleContent::UserAndComputerName),
                 QString("Account condiviso - PC-01 [web locked]"));
        ci->setStudentName("Anna Rossi");ci->setUserInformation({},{},-1);
        QCOMPARE(ComputerCaptionDelegate::captionText(*ci,ComputerListModel::DisplayRoleContent::UserAndComputerName),
                 QString("[Anna Rossi] - PC-01 [web locked]"));
    }
    void webLockedCaptionUsesConfirmedStateOnly() {
        auto ci=client(QUuid::createUuid());
        auto caption=[&] {return ComputerCaptionDelegate::captionText(*ci,ComputerListModel::DisplayRoleContent::ComputerName);};
        for(const auto& status:{"filters active","filtri attivi","pending...","unblocked","error",""}) {
            ci->setNetworkPolicyStatus(status);QCOMPARE(caption(),QString("PC-01"));
        }
        ci->setProperty("schoolPolicyVerifiedState","blocked");QCOMPARE(caption(),QString("PC-01 [web locked]"));
        ci->setNetworkPolicyStatus("pending...");QCOMPARE(caption(),QString("PC-01 [web locked]"));
        ci->setProperty("schoolPolicyVerifiedState","allowed");QCOMPARE(caption(),QString("PC-01"));
        ci->setProperty("schoolPolicyVerifiedState","blocked");ci->stop();QCOMPARE(caption(),QString("PC-01"));
    }
    void webLockedBadgeIsWhiteOnOpaqueRed_data() {
        QTest::addColumn<bool>("attendance");QTest::addColumn<bool>("selected");QTest::addColumn<bool>("wrap");
        for(bool attendance:{false,true}) for(bool selected:{false,true}) for(bool wrap:{false,true})
            QTest::newRow(qPrintable(QString("name-%1-selection-%2-wrap-%3").arg(attendance).arg(selected).arg(wrap)))<<attendance<<selected<<wrap;
    }
    void webLockedBadgeIsWhiteOnOpaqueRed() {
        QFETCH(bool,attendance);QFETCH(bool,selected);QFETCH(bool,wrap);
        QStandardItemModel model;
        const auto prefix=attendance?QString("[Anna Rossi] - "):QString{};
        auto row=new QStandardItem(prefix+"studente - PC-01 [web locked]");
        if(attendance) row->setData("Anna Rossi",ComputerListModel::AttendanceNameRole);
        row->setData(true,ComputerListModel::InternetGuardActiveRole);model.appendRow(row);
        QStyleOptionViewItem option;option.rect=QRect(10,10,600,120);option.font=QFont("Segoe UI",11);
        option.fontMetrics=QFontMetrics(option.font);option.features=QStyleOptionViewItem::HasDisplay;
        if(wrap) option.features|=QStyleOptionViewItem::WrapText;
        option.state=QStyle::State_Enabled|QStyle::State_Active;
        if(selected) option.state|=QStyle::State_Selected;
        option.displayAlignment=Qt::AlignCenter;
        QImage image(620,140,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::magenta);
        ComputerCaptionDelegate delegate;
        {QPainter painter(&image);delegate.paint(&painter,option,model.index(0,0));}
        const auto red=ComputerCaptionDelegate::webLockedBackground();QCOMPARE(red,QColor("#a52a32"));QCOMPARE(red.alpha(),255);
        int redPixels=0,whiteInBadge=0;QRect badge;
        for(int y=0;y<image.height();++y) for(int x=0;x<image.width();++x) if(image.pixelColor(x,y)==red) {++redPixels;badge=badge.united(QRect(x,y,1,1));}
        QVERIFY(redPixels>50);
        for(int y=badge.top();y<=badge.bottom();++y) for(int x=badge.left();x<=badge.right();++x) if(image.pixelColor(x,y)==Qt::white) ++whiteInBadge;
        QVERIFY(whiteInBadge>10);
        row->setData(false,ComputerListModel::InternetGuardActiveRole);row->setText(prefix+"studente - PC-01");
        image.fill(Qt::magenta);{QPainter painter(&image);delegate.paint(&painter,option,model.index(0,0));}
        for(int y=0;y<image.height();++y) for(int x=0;x<image.width();++x) QVERIFY(image.pixelColor(x,y)!=red);
    }
    void modeRequestsAlwaysUseNewSelection_data() {
        QTest::addColumn<QString>("name");
        for(const auto& name:{"ScreenLock","Demo","ShareOwnScreenWindow","ShareOwnScreenFullScreen","ShareUserScreenWindow","ShareUserScreenFullScreen"}) QTest::newRow(name)<<QString(name);
    }
    void modeRequestsAlwaysUseNewSelection() {
        QFETCH(QString,name);
        const Feature mode(name,Feature::Flag::Mode,QUuid::createUuid(),{},name,{},{},{});
        const Feature other("OtherMode",Feature::Flag::Mode,QUuid::createUuid(),{},"Other",{},{},{});
        const Feature guard("InternetGuard",Feature::Flag::Action,QUuid::createUuid(),{},"IG",{},{},{});
        const auto first=client(QUuid::createUuid()),second=client(QUuid::createUuid());
        const FeatureUidList family{mode.uid()};const FeatureList modes{mode,other,guard};
        QVERIFY(SelectedFeatureControl::applies(mode));QVERIFY(SelectedFeatureControl::applies(guard));
        auto apply=[&](const ComputerControlInterfaceList& selected) {
            const auto plan=SelectedFeatureControl::plan(mode,selected,family,modes);
            for(const auto& feature:plan.stop) QVERIFY(feature!=guard);
            for(const auto& ci:plan.clients) ci->setDesignatedModeFeature(plan.start?mode.uid():other.uid());
        };
        apply({first,second});QCOMPARE(first->designatedModeFeature(),mode.uid());QCOMPARE(second->designatedModeFeature(),mode.uid());
        apply({second});QCOMPARE(first->designatedModeFeature(),mode.uid());QCOMPARE(second->designatedModeFeature(),other.uid());
        apply({});QCOMPARE(first->designatedModeFeature(),mode.uid());QCOMPARE(second->designatedModeFeature(),other.uid());
        apply({second});QCOMPARE(first->designatedModeFeature(),mode.uid());QCOMPARE(second->designatedModeFeature(),mode.uid());
        apply({first});QCOMPARE(first->designatedModeFeature(),other.uid());QCOMPARE(second->designatedModeFeature(),mode.uid());
        // Mixed selections start the requested mode on the selected group, not
        // an inverse operation on whichever client happens to be first.
        QVERIFY(SelectedFeatureControl::plan(mode,{first,second},family,modes).start);
        QCOMPARE(SelectedFeatureControl::targets({first,first,{},second}),ComputerControlInterfaceList({first,second}));
        // Stale server feedback cannot undo an explicit unlock designation.
        first->setActiveFeatures({mode.uid()});QVERIFY(!SelectedFeatureControl::active(*first,family));
    }
    void thumbnailBadgeIsLiteralClippedAndThemeAware_data() {
        QTest::addColumn<QString>("mode");QTest::addColumn<bool>("selected");QTest::addColumn<bool>("preloaded");
        for(const auto& mode:{"Light","Dark","HighContrast"}) for(bool selected:{false,true}) for(bool preloaded:{false,true})
            QTest::newRow(qPrintable(QString("%1-%2-%3").arg(mode).arg(selected).arg(preloaded)))<<QString(mode)<<selected<<preloaded;
    }
    void thumbnailBadgeIsLiteralClippedAndThemeAware() {
        QFETCH(QString,mode);QFETCH(bool,selected);QFETCH(bool,preloaded);
        NetLinkAppearance::Preferences preferences;preferences.mode=mode;
        auto palette=NetLinkAppearance::makePalette(preferences,false);
        if(mode=="HighContrast") {
            palette.setColor(QPalette::Base,Qt::black);palette.setColor(QPalette::Midlight,Qt::black);
            palette.setColor(QPalette::Text,Qt::white);palette.setColor(QPalette::Highlight,Qt::yellow);
            palette.setColor(QPalette::HighlightedText,Qt::black);
        }
        QStandardItemModel model;const QString name="<b>Anna & Rossi</b> 学生";
        auto row=new QStandardItem("["+name+"] - SCUOLA\\studente - PC-01");
        row->setData(name,ComputerListModel::AttendanceNameRole);row->setData(preloaded,ComputerListModel::AttendancePreloadedRole);model.appendRow(row);
        QSortFilterProxyModel proxy;proxy.setSourceModel(&model);proxy.setFilterCaseSensitivity(Qt::CaseInsensitive);
        for(const auto& query:{"Anna","studente","PC-01"}) {proxy.setFilterFixedString(query);QCOMPARE(proxy.rowCount(),1);}
        proxy.setFilterFixedString({});
        QImage image(660,180,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::magenta);
        QStyleOptionViewItem option;option.rect=QRect(20,20,620,140);option.palette=palette;
        option.font=QFont("Segoe UI",11);option.fontMetrics=QFontMetrics(option.font);
        option.features=QStyleOptionViewItem::HasDisplay|QStyleOptionViewItem::WrapText;
        option.displayAlignment=Qt::AlignCenter;option.state=QStyle::State_Enabled|QStyle::State_Active;
        if(selected) option.state|=QStyle::State_Selected|QStyle::State_HasFocus;
        ComputerCaptionDelegate delegate;
        {QPainter painter(&image);delegate.paint(&painter,option,proxy.index(0,0));}
        const auto badge=palette.color(selected?QPalette::Base:preloaded?QPalette::Midlight:QPalette::Highlight);
        int badgePixels=0;
        for(int y=0;y<image.height();++y) for(int x=0;x<image.width();++x) {
            if(!option.rect.contains(x,y)) QCOMPARE(image.pixelColor(x,y),QColor(Qt::magenta));
            else if(image.pixelColor(x,y)==badge) ++badgePixels;
        }
        QVERIFY(badgePixels>30);
        // No HTML interpretation: the brackets and tags are part of the plain
        // searchable/accessibility text, not a way to inject a rich-text widget.
        QCOMPARE(proxy.index(0,0).data().toString(),row->text());QVERIFY(row->text().contains("<b>"));
        row->setText("["+QString(120,'W')+"] - studente - PC-01");row->setData(QString(120,'W'),ComputerListModel::AttendanceNameRole);
        option.rect=QRect(20,20,55,40);image.fill(Qt::magenta);
        for(bool wrap:{true,false}) {
            if(!wrap) option.features&=~QStyleOptionViewItem::WrapText;
            image.fill(Qt::magenta);{QPainter painter(&image);delegate.paint(&painter,option,proxy.index(0,0));}
            for(int y=0;y<image.height();++y) for(int x=0;x<image.width();++x)
                if(!option.rect.contains(x,y)) QCOMPARE(image.pixelColor(x,y),QColor(Qt::magenta));
        }
    }
    void thumbnailPreviewUsesActualDelegate() {
        const auto output=qEnvironmentVariable("NETLINK_CAPTION_SCREENSHOT_DIR");
        for(const auto& mode:{"Light","Dark"}) {
            NetLinkAppearance::Preferences preferences;preferences.mode=mode;
            QListView view;view.setPalette(NetLinkAppearance::makePalette(preferences,false));view.setFont(QFont("Segoe UI",10));
            auto style=QStyleFactory::create("Fusion");QVERIFY(style);style->setParent(&view);view.setStyle(style);
            view.setViewMode(QListView::IconMode);view.setResizeMode(QListView::Adjust);view.setSpacing(16);
            view.setIconSize(QSize(280,175));view.setUniformItemSizes(true);view.setWordWrap(true);view.resize(980,300);
            QStandardItemModel model;QPixmap desktop(280,175);desktop.fill(view.palette().color(QPalette::Midlight));
            {QPainter painter(&desktop);painter.setPen(view.palette().color(QPalette::Text));painter.drawText(desktop.rect(),Qt::AlignCenter,"Desktop di prova");}
            for(int i=0;i<3;++i) {
                const auto name=QStringList{"Anna Rossi","Luca Bianchi",""}[i];
                auto caption=name.isEmpty()?QString("studente - PC-03"):QString("[%1] - studente - PC-0%2").arg(name).arg(i+1);
                if(i!=1) caption+=" [web locked]";
                auto row=new QStandardItem(QIcon(desktop),caption);
                row->setData(i!=1,ComputerListModel::InternetGuardActiveRole);
                row->setData(name,ComputerListModel::AttendanceNameRole);row->setData(i==1,ComputerListModel::AttendancePreloadedRole);model.appendRow(row);
            }
            view.setModel(&model);view.setItemDelegate(new ComputerCaptionDelegate(&view));view.show();view.setFocus();QApplication::processEvents();
            for(int i=0;i<3;++i) QVERIFY(view.viewport()->rect().contains(view.visualRect(model.index(i,0))));
            if(!output.isEmpty()) {QVERIFY(QDir().mkpath(output));QVERIFY(view.grab().save(QDir(output).filePath(QString("caption-%1-scale-%2.png").arg(mode,qEnvironmentVariable("QT_SCALE_FACTOR","1")))));}
        }
    }
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
        auto invalid=root;invalid["version"]=3;QTest::newRow("future-version")<<QJsonDocument(invalid).toJson();
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
    void groupSaveIncludesUncheckedPcsButNoOtherRoomOrSearchSubset() {
        DirectoryFixture fixture;ProfileMaster master;
        auto a=client(fixture.a);a->setStudentName("Declared Anna");master.clients={a};master.selected={a};
        ClassProfiles profiles(master);fixture.bind(profiles);
        fixture.search.setFilterFixedString("PC-01");
        fixture.view.setCurrentIndex(fixture.search.index(0,0,fixture.search.index(0,0)));
        SchoolClassProfile snapshot;QString error;
        profiles.m_profile.className="3A";
        QVERIFY2(profiles.groupSnapshot(snapshot,&error),qPrintable(error));
        QCOMPARE(snapshot.assignments.size(),2);QCOMPARE(snapshot.groupId,fixture.roomA);
        QCOMPARE(snapshot.groupName,QString("Room A"));QCOMPARE(snapshot.room,QString("Room A"));
        QCOMPARE(snapshot.assignments[0].computerId,fixture.a);QCOMPARE(snapshot.assignments[0].student,QString("Declared Anna"));
        QCOMPARE(snapshot.assignments[1].computerId,fixture.b);QVERIFY(snapshot.assignments[1].student.isEmpty());
        QVERIFY(!snapshot.toJson().contains(fixture.c.toString(QUuid::WithoutBraces).toUtf8()));
        // A checked other room or an unchecked root does not determine save scope.
        fixture.checked.setCheckedItems({fixture.c});
        QVERIFY(profiles.groupSnapshot(snapshot,&error));QCOMPARE(snapshot.assignments.size(),2);
        fixture.view.setCurrentIndex(fixture.search.index(1,0));
        QVERIFY(profiles.groupSnapshot(snapshot,&error));QCOMPARE(snapshot.assignments.size(),1);QCOMPARE(snapshot.groupId,fixture.roomB);
        fixture.view.selectionModel()->clearSelection();
        QVERIFY(!profiles.groupSnapshot(snapshot,&error));QVERIFY(!error.isEmpty());
    }
    void versionTwoAndLegacyProfilesRoundtripWithStrictGroupFields() {
        DirectoryFixture fixture;auto profile=sample();
        QVERIFY(ClassProfileDirectory::snapshot(fixture.model,fixture.roomA,profile));
        SchoolClassProfile read;QString error;QVERIFY(SchoolClassProfile::fromJson(profile.toJson(),read,&error));
        QCOMPARE(read.groupId,fixture.roomA);QCOMPARE(read.groupName,QString("Room A"));
        QCOMPARE(QJsonDocument::fromJson(profile.toJson()).object()["version"].toInt(),2);
        for(const auto& value:{QJsonValue(),QJsonValue("bad"),QJsonValue("00000000-0000-0000-0000-000000000000")}) {
            auto root=QJsonDocument::fromJson(profile.toJson()).object();root["groupId"]=value;
            QVERIFY(!SchoolClassProfile::fromJson(QJsonDocument(root).toJson(),read,&error));QCOMPARE(read.groupId,fixture.roomA);
        }
        QVERIFY(SchoolClassProfile::fromJson(sample().toJson(),read,&error));QVERIFY(read.groupId.isNull());
    }
    void bulkChecksPreserveTreeIndexesExpansionAndParentState() {
        DirectoryFixture fixture;fixture.view.expandAll();
        const QPersistentModelIndex pc=fixture.checked.index(0,0,fixture.checked.index(0,0));
        QSignalSpy resets(&fixture.checked,&QAbstractItemModel::modelReset);
        QSignalSpy rows(&fixture.checked,&QAbstractItemModel::rowsRemoved);
        fixture.checked.setCheckedItems({fixture.b});
        QVERIFY(pc.isValid());QCOMPARE(pc.data(NetworkObjectModel::UidRole).toUuid(),fixture.a);
        QCOMPARE(resets.count(),0);QCOMPARE(rows.count(),0);QCOMPARE(fixture.model.rowCount(),2);
        QVERIFY(fixture.view.isExpanded(fixture.search.index(0,0)));
        QCOMPARE(pc.data(Qt::CheckStateRole).toInt(),int(Qt::Unchecked));
        QCOMPARE(fixture.checked.index(0,0).data(Qt::CheckStateRole).toInt(),int(Qt::PartiallyChecked));
        QCOMPARE(fixture.checked.index(1,0).data(Qt::CheckStateRole).toInt(),int(Qt::Unchecked));
        fixture.checked.setCheckedItems({fixture.a,fixture.b});
        QCOMPARE(fixture.checked.index(0,0).data(Qt::CheckStateRole).toInt(),int(Qt::Checked));
    }
    void loadChecksMatchedIdentifiersWithoutReplacingOrChangingDirectory() {
        DirectoryFixture fixture;ProfileMaster master;auto ci=client(fixture.a,"192.0.2.99");master.clients={ci};
        ClassProfiles profiles(master);fixture.bind(profiles);auto profile=sample();
        QVERIFY(ClassProfileDirectory::snapshot(fixture.model,fixture.roomA,profile));profile.assignments[0].student="Anna";
        // The stored address is only a hint. A recreated PC with identical IP/name is not matched.
        profile.assignments.append({QUuid::createUuid(),"PC-01","192.0.2.1","Missing student"});
        fixture.model.item(0)->child(0)->setData("192.0.2.99",NetworkObjectModel::HostAddressRole);
        profiles.show("load");QTRY_VERIFY(profiles.m_dialog->findChild<QFileDialog*>("profileFileDialog"));
        const auto path=directory.filePath("group-profile.json");QVERIFY(profile.save(path));QVERIFY(acceptPath(profiles.m_dialog,path));
        QTRY_VERIFY(profiles.m_dialog->findChild<QMessageBox*>("profileLoadConfirmation"));
        auto confirmation=profiles.m_dialog->findChild<QMessageBox*>("profileLoadConfirmation");
        QVERIFY(confirmation->text().contains("2 of 3"));QCOMPARE(confirmation->defaultButton(),confirmation->button(QMessageBox::Cancel));
        confirmation->button(QMessageBox::Yes)->click();
        QTRY_COMPARE(profiles.profile().className,QString("3A"));QCOMPARE(ci->expectedStudentName(),QString("Anna"));
        QCOMPARE(fixture.model.rowCount(),2);QCOMPARE(fixture.model.item(0)->rowCount(),2);QCOMPARE(fixture.model.item(1)->rowCount(),1);
        QCOMPARE(fixture.model.item(0)->child(0)->data(NetworkObjectModel::HostAddressRole).toString(),QString("192.0.2.99"));
        QCOMPARE(fixture.checked.index(1,0).data(Qt::CheckStateRole).toInt(),int(Qt::Unchecked));
        QCOMPARE(fixture.checked.index(0,0).data(Qt::CheckStateRole).toInt(),int(Qt::Checked));
        QCOMPARE(profiles.profile().assignments.size(),3); // Missing assignment stays visible for manual review.
    }
    void pendingLoadRechecksActiveFeaturesAndNeverAppliesOnCancel() {
        DirectoryFixture fixture;ProfileMaster master;ClassProfiles profiles(master);bool safe=true;
        fixture.bind(profiles,[&] {return safe;});profiles.activate(sample());profiles.show();
        auto next=sample();next.className="4B";QVERIFY(ClassProfileDirectory::snapshot(fixture.model,fixture.roomA,next));
        int finished=0;profiles.confirmLoad(next,"test.json",[&] {++finished;});
        auto question=profiles.m_dialog->findChild<QMessageBox*>("profileLoadConfirmation");QVERIFY(question);
        question->button(QMessageBox::Cancel)->click();QCOMPARE(finished,1);QCOMPARE(profiles.profile().className,QString("3A"));
        QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        profiles.confirmLoad(next,"test.json",[&] {++finished;});question=profiles.m_dialog->findChild<QMessageBox*>("profileLoadConfirmation");QVERIFY(question);
        safe=false;question->button(QMessageBox::Yes)->click();QCOMPARE(finished,2);QCOMPARE(profiles.profile().className,QString("3A"));
        QCOMPARE(fixture.checked.index(1,0).data(Qt::CheckStateRole).toInt(),int(Qt::Checked));
    }
    void saveCapturesGroupWithoutChangingClientDeclarations() {
        DirectoryFixture fixture;ProfileMaster master;auto ci=client(fixture.a);ci->setStudentName("Anna");master.clients={ci};
        ClassProfiles profiles(master);fixture.bind(profiles);profiles.m_profile.className="3A";profiles.show("save");
        QTRY_VERIFY(profiles.m_dialog->findChild<QFileDialog*>("profileFileDialog"));
        const auto path=directory.filePath("all-group.json");QVERIFY(acceptPath(profiles.m_dialog,path));
        QTRY_VERIFY(!profiles.m_dirty);QTRY_VERIFY(QFile::exists(path));SchoolClassProfile read;
        QVERIFY(SchoolClassProfile::load(path,read));QCOMPARE(read.assignments.size(),2);QCOMPARE(read.groupId,fixture.roomA);
        QCOMPARE(ci->studentName(),QString("Anna"));QCOMPARE(read.assignments[0].student,QString("Anna"));
    }
    void noHighlightOrNoMatchingPcLeavesTreeAndProfileUnchanged() {
        DirectoryFixture fixture;ProfileMaster master;ClassProfiles profiles(master);fixture.bind(profiles);
        profiles.activate(sample());profiles.show();fixture.view.clearSelection();
        profiles.m_dialog->findChild<QPushButton*>("profileSave")->click();
        QVERIFY(!profiles.m_dialog->findChild<QFileDialog*>("profileFileDialog"));
        QVERIFY(profiles.m_dialog->findChild<QMessageBox*>());
        int finished=0;profiles.confirmLoad(sample(),"missing.json",[&] {++finished;});
        QCOMPARE(finished,1);QCOMPARE(profiles.profile().className,QString("3A"));
        QVERIFY(!profiles.m_dialog->findChild<QMessageBox*>("profileLoadConfirmation"));
        QCOMPARE(fixture.model.rowCount(),2);QCOMPARE(fixture.checked.index(1,0).data(Qt::CheckStateRole).toInt(),int(Qt::Checked));
    }
    void directoryChangeDuringConfirmationAndClosedPromptCannotApply() {
        DirectoryFixture fixture;ProfileMaster master;ClassProfiles profiles(master);fixture.bind(profiles);
        profiles.activate(sample());profiles.show();auto next=sample();next.className="4B";
        QVERIFY(ClassProfileDirectory::snapshot(fixture.model,fixture.roomA,next));
        int finished=0;profiles.confirmLoad(next,"test.json",[&] {++finished;});
        auto question=profiles.m_dialog->findChild<QMessageBox*>("profileLoadConfirmation");QVERIFY(question);
        fixture.model.item(0)->removeRow(1);question->button(QMessageBox::Yes)->click();
        QCOMPARE(finished,1);QCOMPARE(profiles.profile().className,QString("3A"));
        QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        profiles.confirmLoad(next,"test.json",[&] {++finished;});
        question=profiles.m_dialog->findChild<QMessageBox*>("profileLoadConfirmation");QVERIFY(question);
        profiles.m_dialog->hide();question->button(QMessageBox::Yes)->click();
        QCOMPARE(finished,1);QCOMPARE(profiles.profile().className,QString("3A"));
    }
    void clearingAssignmentsKeepsEveryGroupPcAndDoesNotResurrectName() {
        DirectoryFixture fixture;ProfileMaster master;auto ci=client(fixture.a);ci->setStudentName("Anna");master.clients={ci};
        ClassProfiles profiles(master);fixture.bind(profiles);profiles.show();
        auto table=profiles.m_dialog->findChild<QTableWidget*>("profileAssignments");QCOMPARE(table->rowCount(),2);
        table->selectRow(0);profiles.m_dialog->findChild<QPushButton*>("profileRemove")->click();
        QCOMPARE(table->rowCount(),2);QVERIFY(profiles.profile().assignments[0].student.isEmpty());
        SchoolClassProfile snapshot;QString error;QVERIFY(profiles.groupSnapshot(snapshot,&error));
        QCOMPARE(snapshot.assignments.size(),2);QVERIFY(snapshot.assignments[0].student.isEmpty());
        QCOMPARE(ci->studentName(),QString("Anna"));
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
        QVERIFY(!dialog->windowTitle().contains(NetLinkUi::copyrightText()));
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
    void profileSaveButtonShowsPressWithoutSavingOnMouseDown() {
        QSettings settings(directory.filePath("profile-press.ini"),QSettings::IniFormat);
        NetLinkAppearance::Preferences preferences;preferences.mode="Dark";QVERIFY(NetLinkAppearance::save(settings,preferences));
        std::unique_ptr<NetLinkAppearance> appearance(NetLinkAppearance::install(&settings));
        ProfileMaster master;ClassProfiles profiles(master);profiles.activate(sample());profiles.show();
        QTRY_VERIFY(profiles.m_dialog->isVisible());
        const auto button=profiles.m_dialog->findChild<QPushButton*>("profileSave");QVERIFY(button);button->setFocus();
        const auto geometry=button->geometry();const auto normal=button->grab().toImage();QSignalSpy clicked(button,&QPushButton::clicked);
        QTest::mousePress(button,Qt::LeftButton);QVERIFY(button->isDown());QVERIFY(button->grab().toImage()!=normal);
        QCOMPARE(clicked.count(),0);QCOMPARE(button->geometry(),geometry);QVERIFY(!profiles.m_dialog->findChild<QFileDialog*>());
        const auto output=qEnvironmentVariable("NETLINK_PRESS_SCREENSHOTS");
        if(!output.isEmpty()) {QVERIFY(QDir().mkpath(output));QVERIFY(profiles.m_dialog->grab().save(QDir(output).filePath("class-profiles-pressed-scale-"+qEnvironmentVariable("QT_SCALE_FACTOR","1")+".png")));}
        QTest::mouseRelease(button,Qt::LeftButton,Qt::NoModifier,QPoint(-3,-3));QVERIFY(!button->isDown());QCOMPARE(clicked.count(),0);
        QCOMPARE(profiles.profile().className,QString("3A"));
    }
};
QTEST_MAIN(ClassProfileTests)
#include "ClassProfileTests.moc"
