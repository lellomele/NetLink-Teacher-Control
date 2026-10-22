// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "ComputerControlInterface.h"
#include "Feature.h"

// Shared by the toolbar and thumbnail context menu. Never retain a selection
// across user requests and never turn an empty selection into a whole room.
namespace SelectedFeatureControl {
inline bool isMode(const Feature& feature) {
    const auto name=feature.name();
    return feature.testFlag(Feature::Flag::Mode) &&
        (name==QStringLiteral("ScreenLock") || name==QStringLiteral("InputDevicesLock") ||
         name==QStringLiteral("Demo") || name==QStringLiteral("ShareOwnScreenFullScreen") ||
         name==QStringLiteral("ShareOwnScreenWindow") || name==QStringLiteral("ShareUserScreenFullScreen") ||
         name==QStringLiteral("ShareUserScreenWindow"));
}
inline bool applies(const Feature& feature) {
    return isMode(feature) || feature.name()==QStringLiteral("InternetGuard");
}
inline ComputerControlInterfaceList targets(const ComputerControlInterfaceList& selected) {
    ComputerControlInterfaceList result;
    for(const auto& ci:selected) if(ci && !result.contains(ci)) result.append(ci);
    return result;
}
inline bool active(const ComputerControlInterface& ci,const FeatureUidList& family) {
    // A pending explicit mode takes precedence over a delayed server snapshot.
    if(!ci.designatedModeFeature().isNull()) return family.contains(ci.designatedModeFeature());
    for(const auto& uid:family) if(ci.activeFeatures().contains(uid)) return true;
    return false;
}
inline bool allActive(const ComputerControlInterfaceList& clients,const FeatureUidList& family) {
    if(clients.isEmpty()) return false;
    for(const auto& ci:clients) if(!ci || !active(*ci,family)) return false;
    return true;
}
inline bool conflictingDemoSource(const ComputerControlInterfaceList& all,
                                  const ComputerControlInterfaceList& selected,const QString& origin) {
    for(const auto& ci:all) if(ci && !selected.contains(ci)) {
        const auto previous=ci->property("netlinkDemoOrigin").toString();
        if(!previous.isEmpty() && previous!=origin) return true;
    }
    return false;
}
struct Plan {
    ComputerControlInterfaceList clients;
    FeatureList stop;
    bool start=false;
};
inline Plan plan(const Feature& feature,const ComputerControlInterfaceList& selected,
                 const FeatureUidList& family,const FeatureList& modes) {
    Plan result;result.clients=targets(selected);
    if(result.clients.isEmpty()) return result;
    result.start=!allActive(result.clients,family);
    for(const auto& mode:modes) if(mode.testFlag(Feature::Flag::Mode)) result.stop.append(mode);
    return result;
}
}
