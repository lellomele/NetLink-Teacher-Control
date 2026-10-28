// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#include "DiscoveryImportController.h"
#include "HostAddress.h"
#include "NetLinkUi.h"
#include <QJsonObject>
QList<NetworkObject> DiscoveryImportController::groups(const QJsonArray& objects) {
    QHash<QUuid,int> occurrences;for(const auto& value:objects) ++occurrences[NetworkObject(value.toObject()).uid()];
    QList<NetworkObject> result;for(const auto& value:objects) {
        const NetworkObject object(value.toObject());
        if(object.type()==NetworkObject::Type::Location && !object.uid().isNull() && occurrences.value(object.uid())==1) result.append(object);
    }
    return result;
}
QString DiscoveryImportController::validate(const QList<SchoolDiscovery::Record>& selected,const QList<SchoolDiscovery::Record>& available,int port,
    const std::function<bool(const SchoolDiscovery::Record&)>& known,QList<SchoolDiscovery::Record>& current) {
    current.clear();if(selected.isEmpty()) return NetLinkUi::text("Select at least one discovery row. No computer was imported.");
    QSet<QString> addresses,endpoints;QHash<QString,SchoolDiscovery::Record> live;
    for(const auto& peer:available) live.insert(peer.endpoint,peer);
    QList<SchoolDiscovery::Record> validated;
    for(auto record:selected) {
        QString error;const auto peer=live.constFind(record.endpoint);
        if(peer!=live.cend() && peer->conflict) error=SchoolDiscovery::conflictDetails(*peer);
        else if(peer==live.cend() || peer->name!=record.name || peer->address!=record.address || peer->port!=record.port)
            error=NetLinkUi::text("The announcement for %1 (%2:%3) expired, changed, or contains conflicting data. Refresh discovery and select the computer again. No computer was imported.").arg(record.name,record.address,QString::number(record.port));
        if(error.isEmpty()) record=*peer;
        if(error.isEmpty() && known(record)) error=NetLinkUi::text("Computer already configured.");
        if(error.isEmpty() && record.port!=port) error=NetLinkUi::text("Port differs from the configured port: add the computer manually.");
        QSet<QString> aliases;
        for(const auto& address:QStringList{record.address}+record.addresses) {
            const auto key=SchoolDiscovery::addressKey(QHostAddress(address));if(!key.isEmpty()) aliases.insert(key);
        }
        if(error.isEmpty() && (aliases.isEmpty() || endpoints.contains(record.endpoint))) error=NetLinkUi::text("Two selected rows share an observed address. Select only one verified row for this computer.");
        if(error.isEmpty()) for(const auto& address:aliases) if(addresses.contains(address)) {error=NetLinkUi::text("Two selected rows share an observed address. Select only one verified row for this computer.");break;}
        if(!error.isEmpty()) return selected.size()==1?error:NetLinkUi::text("No computers were imported. Check selected row: %1 (%2).\n\n%3\n\nDeselect or resolve the affected row, then repeat the import.").arg(record.name,record.address,error);
        addresses.unite(aliases);endpoints.insert(record.endpoint);validated.append(record);
    }
    current=validated;return {};
}
bool DiscoveryImportController::append(const QJsonArray& before,const Plan& plan,QJsonArray& after,QString& error) {
    after=before;error.clear();bool valid=false;
    for(const auto& group:groups(before)) if(group.uid()==plan.group.uid() && group.name()==plan.group.name()) valid=true;
    if(!valid || plan.computers.isEmpty()) {error=NetLinkUi::text("The destination group in Discovery was changed, renamed or removed. Check the drop-down and repeat the import. No computer was imported.");return false;}
    QSet<QString> addresses;
    for(const auto& value:before) {
        const NetworkObject object(value.toObject());if(object.type()!=NetworkObject::Type::Host) continue;
        const auto key=SchoolDiscovery::addressKey(QHostAddress(HostAddress::parseHost(object.hostAddress())));if(!key.isEmpty()) addresses.insert(key);
    }
    auto candidate=before;
    for(const auto& record:plan.computers) {
        for(const auto& address:QStringList{record.address}+record.addresses) {
            const auto key=SchoolDiscovery::addressKey(QHostAddress(address));
            if(!key.isEmpty() && addresses.contains(key)) {error=NetLinkUi::text("Computer already configured.");return false;}
        }
        for(const auto& address:QStringList{record.address}+record.addresses) addresses.insert(SchoolDiscovery::addressKey(QHostAddress(address)));
        candidate.append(NetworkObject(NetworkObject::Type::Host,record.name.left(120),record.address,{},{},QUuid::createUuid(),plan.group.uid()).toJson());
    }
    after=candidate;return true;
}
