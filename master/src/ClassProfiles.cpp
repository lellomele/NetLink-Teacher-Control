// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include "ClassProfiles.h"
#include "VeyonMasterInterface.h"
#include "NetLinkUi.h"
#include <QtConcurrent>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <functional>

namespace {
QString text(const char* source) { return NetLinkUi::text(source); }
void message(QWidget* parent,const QString& detail) {
    auto box=new QMessageBox(QMessageBox::Information,NetLinkUi::windowTitle(text("Class–room profiles")),detail,QMessageBox::Ok,parent);
    box->setTextFormat(Qt::PlainText);box->setAttribute(Qt::WA_DeleteOnClose);box->open();
}
struct FileResult { bool ok=false; SchoolClassProfile profile; QString error; };
}

ClassProfiles::ClassProfiles(VeyonMasterInterface& master,QObject* parent) : QObject(parent),m_master(&master) {
    m_timer.setInterval(750);
    connect(&m_timer,&QTimer::timeout,this,&ClassProfiles::synchronize);
    m_timer.start();
}
ClassProfiles::~ClassProfiles() { if(m_dialog) delete m_dialog; }

void ClassProfiles::synchronize() {
    if(!m_master) return;
    const auto clients=m_master->allComputerControlInterfaces();
    QHash<QUuid,QString> names;
    for(const auto& seat:m_profile.assignments) names.insert(seat.computerId,seat.student);
    QSet<ComputerControlInterface*> live,seen;
    for(const auto& ci:clients) if(ci) live.insert(ci.data());
    // Do not retain removed controls, or their replies, across a group change.
    for(auto it=m_seen.begin();it!=m_seen.end();) {
        if(!*it) it=m_seen.erase(it);
        else if(!live.contains(it->data())) {
            (*it)->clearStudentDeclaration();(*it)->setExpectedStudentName({});it=m_seen.erase(it);
        } else {seen.insert(it->data());++it;}
    }
    for(const auto& ci:clients) if(ci) {
        if(!seen.contains(ci.data())) {m_seen.append(ci.data());seen.insert(ci.data());}
        ci->setExpectedStudentName(names.value(ci->computer().networkObjectUid()));
    }
}
void ClassProfiles::activate(const SchoolClassProfile& profile) {
    // Even loading the same document starts a fresh naming context. Old client
    // answers must not overwrite this class after a delayed dialog response.
    for(const auto& ci:m_seen) if(ci) {ci->clearStudentDeclaration();ci->setExpectedStudentName({});}
    if(m_master) for(const auto& ci:m_master->allComputerControlInterfaces()) if(ci) ci->clearStudentDeclaration();
    m_profile=profile;synchronize();Q_EMIT assignmentsChanged();
}
void ClassProfiles::changed() {m_dirty=true;synchronize();Q_EMIT assignmentsChanged();}
void ClassProfiles::addSelectedComputers() {
    if(!m_master) return;
    QSet<QUuid> ids;for(const auto& seat:m_profile.assignments) ids.insert(seat.computerId);
    for(const auto& ci:m_master->selectedComputerControlInterfaces()) if(ci && !ci->computer().networkObjectUid().isNull() && !ids.contains(ci->computer().networkObjectUid())) {
        if(m_profile.assignments.size()>=SchoolClassProfile::MaximumAssignments) {message(m_dialog?m_dialog.data():m_master->mainWindow(),text("A profile must contain between 1 and 256 computer assignments."));break;}
        const auto& pc=ci->computer();
        m_profile.assignments.append({pc.networkObjectUid(),pc.name(),pc.hostAddress(),ci->attendanceName()});
        ids.insert(pc.networkObjectUid());
        if(m_profile.room.isEmpty()) m_profile.room=pc.location();
    }
    changed();
}

void ClassProfiles::show(const QString& operation) {
    if(m_dialog) {
        m_dialog->show();m_dialog->raise();m_dialog->activateWindow();
        const auto name=operation=="load"?"profileLoad":operation=="save"?"profileSave":operation=="csv"?"profileCsv":"";
        if(auto button=m_dialog->findChild<QPushButton*>(name)) if(button->isEnabled()) button->click();
        return;
    }
    if(!m_master) return;
    if(m_profile.assignments.isEmpty()) addSelectedComputers();
    auto dialog=new QDialog(m_master->mainWindow());m_dialog=dialog;
    dialog->setObjectName("netlinkClassProfilesDialog");dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowModality(Qt::NonModal);dialog->setWindowTitle(NetLinkUi::windowTitle(text("Class–room profiles")));
    dialog->setWindowIcon(QIcon(":/core/veyon.png"));dialog->resize(900,560);
    auto layout=new QVBoxLayout(dialog);
    auto help=new QLabel(text("Profiles store computer–student assignments on this Master only. Preloaded names and client declarations are not verified attendance. To read a name again, select the computers in the Console and use Attendance → Request names."),dialog);
    help->setTextFormat(Qt::PlainText);help->setWordWrap(true);layout->addWidget(help);
    auto editor=new QWidget(dialog);layout->addWidget(editor,1);auto editLayout=new QVBoxLayout(editor);editLayout->setContentsMargins(0,0,0,0);
    auto form=new QFormLayout;editLayout->addLayout(form);
    auto className=new QLineEdit(m_profile.className,editor);className->setObjectName("profileClass");className->setMaxLength(80);
    auto room=new QLineEdit(m_profile.room,editor);room->setObjectName("profileRoom");room->setMaxLength(80);
    form->addRow(text("Class"),className);form->addRow(text("Room"),room);
    auto table=new QTableWidget(editor);table->setObjectName("profileAssignments");table->setColumnCount(4);
    table->setHorizontalHeaderLabels({text("Computer"),text("Assigned student (editable)"),text("Client declaration"),text("Name source / connection")});
    table->setSelectionBehavior(QAbstractItemView::SelectRows);table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);table->horizontalHeader()->setStretchLastSection(true);
    table->setColumnWidth(0,180);table->setColumnWidth(1,200);table->setColumnWidth(2,180);editLayout->addWidget(table,1);
    auto actions=new QDialogButtonBox(editor);editLayout->addWidget(actions);
    auto add=actions->addButton(text("Add selected PCs"),QDialogButtonBox::ActionRole);add->setObjectName("profileAdd");
    add->setToolTip(text("Adds the computers currently selected in the Console, without changing their addresses or authentication."));
    auto remove=actions->addButton(text("Remove assignments"),QDialogButtonBox::ActionRole);remove->setObjectName("profileRemove");
    auto use=actions->addButton(text("Use declared names"),QDialogButtonBox::ActionRole);use->setObjectName("profileUseDeclared");
    use->setToolTip(text("Copies the selected rows' client declarations into the editable profile. Save JSON to keep them for another lesson."));
    auto files=new QDialogButtonBox(editor);editLayout->addWidget(files);
    auto fresh=files->addButton(text("New / unload profile"),QDialogButtonBox::ActionRole);fresh->setObjectName("profileNew");
    auto load=files->addButton(text("Load JSON…"),QDialogButtonBox::ActionRole);load->setObjectName("profileLoad");
    auto save=files->addButton(text("Save JSON…"),QDialogButtonBox::ActionRole);save->setObjectName("profileSave");
    auto csv=files->addButton(text("Export CSV…"),QDialogButtonBox::ActionRole);csv->setObjectName("profileCsv");
    auto status=new QLabel(dialog);status->setObjectName("profileStatus");status->setTextFormat(Qt::PlainText);status->setWordWrap(true);layout->addWidget(status);
    auto close=new QDialogButtonBox(QDialogButtonBox::Close,dialog);layout->addWidget(close);connect(close,&QDialogButtonBox::rejected,dialog,&QDialog::reject);

    auto refresh=[this,table,status] {
        QHash<QUuid,ComputerControlInterface::Pointer> clients;
        if(m_master) for(const auto& ci:m_master->allComputerControlInterfaces()) if(ci) clients.insert(ci->computer().networkObjectUid(),ci);
        const QSignalBlocker blocked(table);
        if(table->rowCount()!=m_profile.assignments.size()) table->setRowCount(m_profile.assignments.size());
        int unmatched=0;
        for(int row=0;row<m_profile.assignments.size();++row) {
            const auto& seat=m_profile.assignments[row];const auto ci=clients.value(seat.computerId);
            if(!ci) ++unmatched;
            const QStringList values{ci?ci->computer().name():seat.computerName,seat.student,ci?ci->studentName():QString{},
                ci?ci->attendanceStatus()+" / "+text(ci->state()==ComputerControlInterface::State::Connected?"online":"offline"):text("PC not displayed / not configured")};
            for(int column=0;column<4;++column) {
                auto item=table->item(row,column);
                if(!item) {item=new QTableWidgetItem;item->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable|(column==1?Qt::ItemIsEditable:Qt::NoItemFlags));table->setItem(row,column,item);}
                if(item->text()!=values[column]) item->setText(values[column]);
                item->setToolTip(seat.computerId.toString(QUuid::WithoutBraces)+"\n"+(ci?ci->computer().hostAddress():seat.hostHint));
            }
        }
        status->setText((m_dirty?text("Unsaved assignments — kept in memory until the Master closes."):text("Profile saved / loaded."))+"\n"+
            text("Unmatched or hidden PCs: %1. Select the correct group in Locations & computers; names are never matched by IP or hostname.").arg(unmatched));
    };
    auto rebuild=[this,className,room,refresh] {const QSignalBlocker a(className),b(room);className->setText(m_profile.className);room->setText(m_profile.room);refresh();};
    auto selectedRows=[table] {QList<int> rows;for(const auto& index:table->selectionModel()->selectedRows()) rows.append(index.row());std::sort(rows.begin(),rows.end(),std::greater<int>());return rows;};
    connect(className,&QLineEdit::textEdited,dialog,[this,refresh](const QString& value) {m_profile.className=value;changed();refresh();});
    connect(room,&QLineEdit::textEdited,dialog,[this,refresh](const QString& value) {m_profile.room=value;changed();refresh();});
    connect(table,&QTableWidget::cellChanged,dialog,[this,table,refresh,dialog](int row,int column) {
        if(column!=1 || row<0 || row>=m_profile.assignments.size()) return;
        const auto name=table->item(row,column)->text().trimmed();
        if(!SchoolClassProfile::validText(name,120)) {refresh();message(dialog,text("Invalid assignment text: student names may contain up to 120 characters."));return;}
        m_profile.assignments[row].student=name;changed();refresh();
    });
    connect(add,&QPushButton::clicked,dialog,[this,rebuild] {addSelectedComputers();rebuild();});
    connect(remove,&QPushButton::clicked,dialog,[this,selectedRows,refresh] {for(int row:selectedRows()) if(row<m_profile.assignments.size()) m_profile.assignments.removeAt(row);changed();refresh();});
    connect(use,&QPushButton::clicked,dialog,[this,selectedRows,refresh] {
        if(!m_master) return;
        for(int row:selectedRows()) for(const auto& ci:m_master->allComputerControlInterfaces()) if(ci && ci->computer().networkObjectUid()==m_profile.assignments[row].computerId && !ci->studentName().isEmpty()) m_profile.assignments[row].student=ci->studentName();
        changed();refresh();
    });
    // Replacing unsaved assignments always needs an explicit choice. Closing the
    // panel is harmless: its draft and active assignments live in the Master.
    auto replace=[this,dialog,editor](const std::function<void()>& action) {
        if(!m_dirty) {action();return;}
        auto box=new QMessageBox(QMessageBox::Question,NetLinkUi::windowTitle(text("Class–room profiles")),text("Replace the unsaved assignments? Save JSON first if you want to keep them."),QMessageBox::Discard|QMessageBox::Cancel,dialog);
        box->setDefaultButton(QMessageBox::Cancel);box->setTextFormat(Qt::PlainText);box->setAttribute(Qt::WA_DeleteOnClose);editor->setEnabled(false);
        connect(box,&QMessageBox::finished,dialog,[editor,action](int result) {editor->setEnabled(true);if(result==QMessageBox::Discard) action();});box->open();
    };
    connect(fresh,&QPushButton::clicked,dialog,[this,replace,rebuild] {replace([this,rebuild] {activate({});m_path.clear();m_dirty=false;rebuild();});});
    auto choose=[this,dialog,editor,rebuild,status](bool saving,bool exportCsv) {
        if(saving && !exportCsv) {QString error;if(!m_profile.validate(&error)) {message(dialog,error);return;}}
        auto initial=m_path;
        if(initial.isEmpty()) {initial=SchoolClassProfile::defaultDirectory();if(!QDir(initial).exists()) initial=QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);}
        if(exportCsv && !m_path.isEmpty()) initial=QFileInfo(m_path).absolutePath();
        auto chooser=new QFileDialog(dialog,saving?text("Save class-room profile"):text("Load class-room profile"),initial,exportCsv?"CSV (*.csv)":"JSON (*.json)");
        chooser->setObjectName("profileFileDialog");chooser->setOption(QFileDialog::DontUseNativeDialog);chooser->setAcceptMode(saving?QFileDialog::AcceptSave:QFileDialog::AcceptOpen);
        chooser->setFileMode(saving?QFileDialog::AnyFile:QFileDialog::ExistingFile);chooser->setDefaultSuffix(exportCsv?"csv":"json");chooser->setAttribute(Qt::WA_DeleteOnClose);
        editor->setEnabled(false);
        connect(chooser,&QFileDialog::rejected,dialog,[editor] {editor->setEnabled(true);});
        connect(chooser,&QFileDialog::fileSelected,dialog,[this,dialog,editor,rebuild,status,saving,exportCsv](const QString& path) {
            if(path.isEmpty()) {editor->setEnabled(true);return;}
            status->setText(text("Reading / writing file… The Console remains available."));
            const auto snapshot=m_profile;
            // Capture CSV before entering the worker: no QObject or live client
            // may be read from another thread. Formula-like fields are quoted.
            QByteArray csvData("\xef\xbb\xbf");
            auto quote=[](QString value) {const auto trimmed=value.trimmed();if(!trimmed.isEmpty() && QString("=+-@").contains(trimmed.front())) value.prepend('\'');value.replace('"',"\"\"");return '"'+value+'"';};
            csvData+=text("Class;Room;Computer ID;Computer;Host hint;Assigned student;Client declaration;Name source;Connection\r\n").toUtf8();
            for(const auto& seat:snapshot.assignments) {
                ComputerControlInterface::Pointer live;
                if(m_master) for(const auto& ci:m_master->allComputerControlInterfaces()) if(ci && ci->computer().networkObjectUid()==seat.computerId) {live=ci;break;}
                QStringList cells{snapshot.className,snapshot.room,seat.computerId.toString(QUuid::WithoutBraces),live?live->computer().name():seat.computerName,live?live->computer().hostAddress():seat.hostHint,seat.student,live?live->studentName():QString{},live?live->attendanceStatus():text("Preloaded name"),text(live && live->state()==ComputerControlInterface::State::Connected?"online":"offline")};
                for(auto& cell:cells) cell=quote(cell);
                csvData+=(cells.join(';')+"\r\n").toUtf8();
            }
            auto watcher=new QFutureWatcher<FileResult>(dialog);
            connect(watcher,&QFutureWatcher<FileResult>::finished,dialog,[this,dialog,editor,rebuild,watcher,saving,exportCsv,path] {
                const auto result=watcher->result();watcher->deleteLater();
                // reject() hides before deferred deletion; queued completion in
                // that short gap must not apply a file to a closed/new session.
                if(m_dialog!=dialog || !dialog->isVisible()) return;
                editor->setEnabled(true);
                if(!result.ok) {message(dialog,result.error);rebuild();return;}
                if(!saving) activate(result.profile);
                if(!exportCsv) {m_path=path;m_dirty=false;}
                rebuild();
            });
            watcher->setFuture(QtConcurrent::run([path,snapshot,saving,exportCsv,csvData] {
                FileResult result;
                if(exportCsv) {QSaveFile file(path);file.setDirectWriteFallback(false);result.ok=file.open(QIODevice::WriteOnly) && file.setPermissions(QFile::ReadOwner|QFile::WriteOwner) && file.write(csvData)==csvData.size() && file.commit();if(!result.ok) result.error=text("Export failed.");}
                else if(saving) result.ok=snapshot.save(path,&result.error);
                else result.ok=SchoolClassProfile::load(path,result.profile,&result.error);
                return result;
            }));
        });chooser->open();
    };
    connect(load,&QPushButton::clicked,dialog,[replace,choose] {replace([choose] {choose(false,false);});});
    connect(save,&QPushButton::clicked,dialog,[choose] {choose(true,false);});
    connect(csv,&QPushButton::clicked,dialog,[choose] {choose(true,true);});
    connect(&m_timer,&QTimer::timeout,dialog,refresh);
    rebuild();dialog->show();
    if(operation=="load") QTimer::singleShot(0,load,&QPushButton::click);
    else if(operation=="save") QTimer::singleShot(0,save,&QPushButton::click);
    else if(operation=="csv") QTimer::singleShot(0,csv,&QPushButton::click);
}
