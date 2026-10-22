// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#pragma once
#include "VeyonCore.h"
#include <QStringList>

namespace ServiceNetworkAddresses {
// Pure selection among addresses on which the listener can actually accept.
// Prefer an authenticated IPv4 connection, then its interface's IPv4 address,
// the OS route, and active Ethernet/Wi-Fi. IPv6-only/local-only remain valid.
VEYON_CORE_EXPORT QString preferred(const QStringList& available,
    const QStringList& authenticated = {}, const QString& routedIPv4 = {},
    const QStringList& physicalIPv4 = {}, const QStringList& connectedInterfaceIPv4 = {});
// Local routing-table lookup only: never sends packets or performs DNS.
VEYON_CORE_EXPORT QString routedIPv4();
}
