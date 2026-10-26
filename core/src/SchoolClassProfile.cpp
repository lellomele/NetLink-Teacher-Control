// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include "SchoolClassProfile.h"
#include "NetLinkUi.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>

namespace {
bool fail(QString* error, const char* message) {
    if(error) *error=NetLinkUi::text(message);
    return false;
}
bool fields(const QJsonObject& object, const QSet<QString>& allowed) {
    for(auto it=object.begin();it!=object.end();++it) if(!allowed.contains(it.key())) return false;
    return true;
}
}
bool SchoolClassProfile::validText(const QString& text,int maximum,bool allowEmpty) {
    if(text.size()>maximum || (!allowEmpty && text.trimmed().isEmpty())) return false;
    for(int i=0;i<text.size();++i) {
        const auto c=text[i];
        if(c.category()==QChar::Other_Control || c.category()==QChar::Other_Format) return false;
        if(c.isHighSurrogate()) {if(++i>=text.size() || !text[i].isLowSurrogate()) return false;}
        else if(c.isLowSurrogate()) return false;
    }
    return true;
}
bool SchoolClassProfile::validate(QString* error) const {
    if(error) error->clear();
    if(!validText(className,80,false) || !validText(room,80,false)) return fail(error,"Enter a class and a room (maximum 80 characters each).");
    if(assignments.isEmpty() || assignments.size()>MaximumAssignments) return fail(error,"A profile must contain between 1 and 256 computer assignments.");
    if((groupId.isNull() && !groupName.isEmpty()) || (!groupId.isNull() && !validText(groupName,256,false))) return fail(error,"Invalid group identifier or name in the profile.");
    QSet<QUuid> ids;
    for(const auto& seat:assignments) {
        if(seat.computerId.isNull() || ids.contains(seat.computerId)) return fail(error,"Invalid or duplicate computer identifier in the profile.");
        ids.insert(seat.computerId);
        if(!validText(seat.computerName,256,false) || !validText(seat.hostHint,256) || !validText(seat.student,120)) return fail(error,"Invalid assignment text: student names may contain up to 120 characters.");
    }
    return true;
}
QByteArray SchoolClassProfile::toJson() const {
    QJsonArray seats;
    for(const auto& seat:assignments) seats.append(QJsonObject{{"computerId",seat.computerId.toString(QUuid::WithoutBraces)},
        {"computerName",seat.computerName},{"hostHint",seat.hostHint},{"student",seat.student}});
    QJsonObject root{{"format","netlink-class-room"},{"version",groupId.isNull()?1:2},
        {"class",className},{"room",room},{"assignments",seats}};
    if(!groupId.isNull()) {root.insert("groupId",groupId.toString(QUuid::WithoutBraces));root.insert("groupName",groupName);}
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}
bool SchoolClassProfile::fromJson(const QByteArray& data,SchoolClassProfile& profile,QString* error) {
    if(error) error->clear();
    if(data.size()>MaximumBytes) return fail(error,"Profile exceeds the 1 MiB size limit.");
    QJsonParseError parse;const auto doc=QJsonDocument::fromJson(data,&parse);
    if(parse.error!=QJsonParseError::NoError || !doc.isObject()) return fail(error,"Invalid profile JSON. The active profile has not changed.");
    const auto root=doc.object();
    const auto version=root["version"].toDouble();
    const bool grouped=version==2;
    auto allowed=QSet<QString>{"format","version","class","room","assignments"};
    if(grouped) {allowed.insert("groupId");allowed.insert("groupName");}
    if(!fields(root,allowed) || root["format"]!="netlink-class-room" ||
       !root["version"].isDouble() || (version!=1 && version!=2) || !root["class"].isString() ||
       !root["room"].isString() || !root["assignments"].isArray()) return fail(error,"Unsupported profile format or version. Use a NetLink class-room JSON, not a Configurator backup.");
    const auto seats=root["assignments"].toArray();
    if(seats.size()>MaximumAssignments) return fail(error,"A profile must contain between 1 and 256 computer assignments.");
    SchoolClassProfile candidate;candidate.className=root["class"].toString();candidate.room=root["room"].toString();
    if(grouped) {
        if(!root["groupId"].isString() || root["groupId"].toString().size()!=36 || QUuid(root["groupId"].toString()).isNull() || !root["groupName"].isString()) return fail(error,"Invalid group identifier or name in the profile.");
        candidate.groupId=QUuid(root["groupId"].toString());candidate.groupName=root["groupName"].toString();
    }
    for(const auto& value:seats) {
        if(!value.isObject()) return fail(error,"Invalid assignment in the profile.");
        const auto seat=value.toObject();
        if(!fields(seat,{"computerId","computerName","hostHint","student"}) || !seat["computerId"].isString() ||
           seat["computerId"].toString().size()!=36 || !seat["computerName"].isString() ||
           !seat["hostHint"].isString() || !seat["student"].isString()) return fail(error,"Invalid assignment in the profile.");
        candidate.assignments.append({QUuid(seat["computerId"].toString()),seat["computerName"].toString(),seat["hostHint"].toString(),seat["student"].toString()});
    }
    if(!candidate.validate(error)) return false;
    profile=std::move(candidate);return true;
}
bool SchoolClassProfile::load(const QString& path,SchoolClassProfile& profile,QString* error) {
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly)) return fail(error,"Cannot read the profile file. Check its path and permissions.");
    const auto data=file.read(MaximumBytes+1);
    if(file.error()!=QFile::NoError) return fail(error,"Cannot read the profile file. Check its path and permissions.");
    return fromJson(data,profile,error);
}
bool SchoolClassProfile::save(const QString& path,QString* error) const {
    if(!validate(error)) return false;
    const auto data=toJson();
    if(data.size()>MaximumBytes) return fail(error,"Profile exceeds the 1 MiB size limit.");
    QSaveFile file(path);file.setDirectWriteFallback(false);
    if(!file.open(QIODevice::WriteOnly) || !file.setPermissions(QFile::ReadOwner|QFile::WriteOwner) ||
       file.write(data)!=data.size() || !file.commit()) return fail(error,"Cannot save the profile. The previous file has not been replaced.");
    return true;
}
QString SchoolClassProfile::defaultDirectory() {
#ifdef Q_OS_WIN
    // Do not use the legacy Veyon expansion of %APPDATA%.
    const auto root=qEnvironmentVariable("APPDATA");
    if(!root.isEmpty() && QDir::isAbsolutePath(root)) return QDir(root).filePath("NetLink/Classi");
#endif
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)).filePath("NetLink/Classi");
}
