// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#pragma once
#include "FeatureMessage.h"

namespace ServiceDiagnostics {
enum Command { Query=1, Reply=2 };
enum class Backend { Unchecked, TcpAvailable, Ready, Connecting, Unreachable };
struct Snapshot {
    QString plugin;
    bool external=false;
    int internalPort=0;
    Backend backend=Backend::Unchecked;
    qint64 frameAgeMs=-1;
};
// Additive extension on the existing authenticated feature-message connection.
VEYON_CORE_EXPORT QUuid uid();
VEYON_CORE_EXPORT bool validQuery(const FeatureMessage& message);
VEYON_CORE_EXPORT QByteArray encode(const Snapshot& snapshot);
VEYON_CORE_EXPORT bool decode(const QByteArray& bytes,Snapshot* snapshot);
VEYON_CORE_EXPORT QString tooltip(const Snapshot& snapshot,qint64 elapsedMs=0);

class VEYON_CORE_EXPORT ClientStatus {
public:
    enum class Phase { Disconnected, Waiting, Ready, Unavailable, Invalid };
    static constexpr int QueryInterval=15000,ReplyTimeout=5000,RetryInterval=60000,MaximumAge=35000;
    void reset(bool connected=false);
    QString request(qint64 now);
    bool expire(qint64 now);
    bool receive(const FeatureMessage& message,qint64 now);
    QString tooltip(qint64 now) const;
    Phase phase() const {return m_phase;}
private:
    Phase m_phase=Phase::Disconnected;
    Snapshot m_snapshot;
    QString m_pending;
    qint64 m_deadline=-1,m_nextQuery=0,m_received=-1;
};
}
