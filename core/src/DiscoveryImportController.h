// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#pragma once
#include "NetworkObject.h"
#include "SchoolDiscovery.h"
#include <QJsonArray>
#include <functional>
// Pure import policy: no widgets, DNS requests, settings writes or client changes.
class VEYON_CORE_EXPORT DiscoveryImportController {
public:
    struct Plan {NetworkObject group;QList<SchoolDiscovery::Record> computers;};
    static QList<NetworkObject> groups(const QJsonArray& objects);
    static bool createGroup(const QJsonArray& before,const QString& name,QJsonArray& after,NetworkObject& group,QString& error);
    static QString validate(const QList<SchoolDiscovery::Record>& selected,
        const QList<SchoolDiscovery::Record>& live,int port,
        const std::function<bool(const SchoolDiscovery::Record&)>& known,
        QList<SchoolDiscovery::Record>& current);
    static bool append(const QJsonArray& before,const Plan& plan,QJsonArray& after,QString& error);
};
