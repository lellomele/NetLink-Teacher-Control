// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include "ServiceNetworkAddresses.h"
#include <QHostAddress>
#ifdef Q_OS_WIN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#endif

namespace {
QString canonical(const QString& value) {
    QHostAddress address(value);
    bool ipv4=false;const auto v4=address.toIPv4Address(&ipv4);
    if(ipv4) address=QHostAddress(v4);
    if(address.isNull() || address.isMulticast() || address==QHostAddress::Any ||
       address==QHostAddress::AnyIPv4 || address==QHostAddress::AnyIPv6 ||
       address==QHostAddress::Broadcast) return {};
    return address.toString();
}
bool usableIPv4(const QString& value) {
    const QHostAddress address(value);bool ipv4=false;
    const auto v4=address.toIPv4Address(&ipv4);
    return ipv4 && !address.isLoopback() && (v4&0xffff0000U)!=0xa9fe0000U;
}
}

QString ServiceNetworkAddresses::preferred(const QStringList& available,
    const QStringList& authenticated, const QString& routedIPv4,
    const QStringList& physicalIPv4, const QStringList& connectedInterfaceIPv4) {
    QStringList addresses;
    for(const auto& value:available) {
        const auto address=canonical(value);
        if(!address.isEmpty() && !addresses.contains(address)) addresses.append(address);
    }
    const auto firstIPv4=[&](const QStringList& candidates) -> QString {
        for(const auto& value:candidates) {
            const auto address=canonical(value);
            if(addresses.contains(address) && usableIPv4(address)) return address;
        }
        return {};
    };
    for(const auto& candidates:{authenticated,connectedInterfaceIPv4,QStringList{routedIPv4},physicalIPv4,addresses}) {
        const auto address=firstIPv4(candidates);if(!address.isEmpty()) return address;
    }
    // Link-local IPv4 has no general route but is still preferable to unrelated
    // IPv6/loopback when it is the only IPv4 actually assigned to this listener.
    for(const auto& value:addresses) {
        const QHostAddress address(value);
        if(address.protocol()==QAbstractSocket::IPv4Protocol && !address.isLoopback()) return value;
    }
    for(const auto& value:authenticated) {
        const auto address=canonical(value);
        if(addresses.contains(address) && !QHostAddress(address).isLoopback()) return address;
    }
    for(const auto& value:addresses) if(!QHostAddress(value).isLoopback()) return value;
    return addresses.isEmpty()?QString{}:addresses.first();
}

QString ServiceNetworkAddresses::routedIPv4() {
#ifdef Q_OS_WIN
    // Ask Windows which source address it would use for a non-local route.
    // TEST-NET-2 is only a routing-table input; no socket/connection is created.
    SOCKADDR_INET destination{},source{};destination.Ipv4.sin_family=AF_INET;
    destination.Ipv4.sin_addr.s_addr=htonl(0xc6336401U);
    MIB_IPFORWARD_ROW2 route{};
    if(GetBestRoute2(nullptr,0,nullptr,&destination,0,&route,&source)==NO_ERROR && source.si_family==AF_INET)
        return QHostAddress(ntohl(source.Ipv4.sin_addr.s_addr)).toString();
#endif
    return {};
}
