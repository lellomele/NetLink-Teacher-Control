// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include "NetworkObject.h"
#include "NetLinkUi.h"
#include "AuthenticationCredentials.h"
#include <functional>

// Pure plans: no settings writes, key-file operations or network requests.
namespace SchoolDirectoryOperations {
struct KeyChoice {QString name,error;bool explicitKey=false;};
inline KeyChoice keyChoice(const QJsonArray& objects,const QJsonObject& bindings,
                           const QUuid& computerId,const QString& defaultKey,
                           const QString& environmentKey,const QString& legacyKey) {
    KeyChoice result;QUuid group;int matches=0;
    if(!computerId.isNull()) for(const auto& value:objects) {
        const NetworkObject object(value.toObject());
        if(object.uid()==computerId && object.type()==NetworkObject::Type::Host) {group=object.parentUid();++matches;}
    }
    if(matches>1) {result.error=NetLinkUi::text("Ambiguous computer identifier: authentication key cannot be selected.");return result;}
    const auto binding=bindings.value(group.toString(QUuid::WithoutBraces));
    if(!group.isNull() && !binding.isUndefined()) {
        result.explicitKey=true;
        if(!binding.isString() || binding.toString().isEmpty()) {result.error=NetLinkUi::text("The destination group's authentication key is invalid.");return result;}
        result.name=binding.toString();
    } else if(!environmentKey.isEmpty()) {result.name=environmentKey;result.explicitKey=true;}
    else if(!defaultKey.isEmpty()) {result.name=defaultKey;result.explicitKey=true;}
    else result.name=legacyKey;
    if(!result.name.isEmpty() && (!VeyonCore::isAuthenticationKeyNameValid(result.name) || result.name.size()>255))
        result.error=NetLinkUi::text("Invalid authentication key name. No other key will be tried.");
    return result;
}
inline AuthenticationCredentials credentials(const KeyChoice& choice,QString& error,
    const std::function<bool(const QString&,AuthenticationCredentials&)>& load) {
    error=choice.error;AuthenticationCredentials candidate;
    if(error.isEmpty() && !choice.name.isEmpty() && load(choice.name,candidate)) {
        candidate.setAuthenticationKeyName(choice.name);return candidate;
    }
    if(error.isEmpty()) error=NetLinkUi::text("Private key %1 is missing, unreadable or invalid. No other key will be tried.").arg(choice.name);
    return {}; // A partially failed loader must never retain another key/token.
}
inline bool move(const QJsonArray& before,const QSet<QUuid>& computers,const QUuid& destination,
                 QJsonArray& after,QString& error) {
    error.clear();after=before;
    auto fail=[&](const char* message){error=NetLinkUi::text(message);return false;};
    if(computers.isEmpty() || computers.contains(QUuid{}) || destination.isNull()) return fail("Select computers and a destination group.");
    QSet<QUuid> seen,found;bool validDestination=false;
    for(const auto& value:before) {
        const NetworkObject object(value.toObject());
        if(object.uid().isNull() || seen.contains(object.uid())) return fail("Duplicate or invalid object identifiers. No computer was moved.");
        seen.insert(object.uid());
        if(object.uid()==destination) validDestination=object.type()==NetworkObject::Type::Location;
        if(computers.contains(object.uid())) {
            if(object.type()!=NetworkObject::Type::Host) return fail("Only computer rows can be moved.");
            found.insert(object.uid());
        }
    }
    if(!validDestination || found!=computers) return fail("The computers or destination group changed. No computer was moved.");
    for(int i=0;i<after.size();++i) {
        auto json=after[i].toObject();
        if(computers.contains(QUuid(json.value(QStringLiteral("Uid")).toString()))) {
            json.insert(QStringLiteral("ParentUid"),destination.toString(QUuid::WithoutBraces));after[i]=json;
        }
    }
    return true;
}
}
