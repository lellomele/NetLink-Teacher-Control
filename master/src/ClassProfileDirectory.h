// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#pragma once
#include "SchoolClassProfile.h"
#include "NetworkObjectModel.h"
#include "CheckableItemProxyModel.h"
#include <QSet>
#include <functional>

// Read the configured directory, not the thumbnail selection or search proxy.
// Applying a profile changes check states only; it never imports network objects.
namespace ClassProfileDirectory {
inline NetworkObject::Type type(const QModelIndex& index) {
    return NetworkObject::Type(index.data(NetworkObjectModel::TypeRole).toInt());
}
inline QUuid highlightedGroup(QModelIndex index) {
    for(;index.isValid();index=index.parent())
        if(type(index)==NetworkObject::Type::Location || type(index)==NetworkObject::Type::DesktopGroup)
            return index.data(NetworkObjectModel::UidRole).toUuid();
    return {};
}
inline void visit(QAbstractItemModel& model,const QModelIndex& parent,
                  const std::function<void(const QModelIndex&)>& action,int depth=0) {
    if(depth>64) return; // Defensive limit for directory plugins.
    if(model.canFetchMore(parent)) model.fetchMore(parent);
    const auto rows=model.rowCount(parent);
    for(int row=0;row<rows;++row) {
        const auto index=model.index(row,0,parent);
        action(index);
        if(NetworkObject::isContainer(type(index))) visit(model,index,action,depth+1);
    }
}
inline QModelIndex find(QAbstractItemModel& model,const QUuid& id) {
    QModelIndex result;
    visit(model,{},[&](const QModelIndex& index) {
        if(index.data(NetworkObjectModel::UidRole).toUuid()==id) result=index;
    });
    return result;
}
inline bool snapshot(QAbstractItemModel& directory,const QUuid& groupId,
                     SchoolClassProfile& profile) {
    if(groupId.isNull()) return false;
    const auto group=find(directory,groupId);
    if(!group.isValid() || (type(group)!=NetworkObject::Type::Location && type(group)!=NetworkObject::Type::DesktopGroup)) return false;
    QVector<SchoolClassProfile::Assignment> seats;
    QSet<QUuid> seen;
    visit(directory,group,[&](const QModelIndex& index) {
        // A nested room is a separate group, not part of the highlighted room.
        if(type(index)!=NetworkObject::Type::Host || highlightedGroup(index)!=groupId) return;
        const auto id=index.data(NetworkObjectModel::UidRole).toUuid();
        if(id.isNull() || seen.contains(id)) return;
        seen.insert(id);
        seats.append({id,index.data(NetworkObjectModel::NameRole).toString(),
                      index.data(NetworkObjectModel::HostAddressRole).toString(),{}});
    });
    profile.groupId=groupId;
    profile.groupName=group.data(NetworkObjectModel::NameRole).toString();
    if(profile.room.isEmpty() || profile.assignments.isEmpty()) profile.room=profile.groupName;
    profile.assignments=seats;
    return true;
}
struct LoadPlan {
    QSet<QUuid> selectable;
    QStringList unavailable;
    QStringList moved;
    QHash<QUuid,QUuid> currentGroups;
    bool operator==(const LoadPlan& other) const {return selectable==other.selectable && unavailable==other.unavailable && moved==other.moved && currentGroups==other.currentGroups;}
};
inline LoadPlan plan(QAbstractItemModel& checked,const SchoolClassProfile& profile) {
    QSet<QUuid> available;QHash<QUuid,QUuid> groups;
    visit(checked,{},[&](const QModelIndex& index) {
        if(type(index)==NetworkObject::Type::Host && (checked.flags(index)&Qt::ItemIsUserCheckable)) {
            available.insert(index.data(NetworkObjectModel::UidRole).toUuid());
            groups.insert(index.data(NetworkObjectModel::UidRole).toUuid(),highlightedGroup(index));
        }
    });
    LoadPlan result;
    for(const auto& seat:profile.assignments) {
        if(available.contains(seat.computerId)) {
            result.selectable.insert(seat.computerId);
            result.currentGroups.insert(seat.computerId,groups.value(seat.computerId));
            if(!profile.groupId.isNull() && groups.value(seat.computerId)!=profile.groupId) result.moved.append(seat.computerName);
        }
        else result.unavailable.append(seat.computerName);
    }
    return result;
}
}
