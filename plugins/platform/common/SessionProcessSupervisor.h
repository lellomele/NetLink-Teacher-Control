// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#pragma once
#include <algorithm>
#include <map>
#include <memory>

// Owning session/process association. Presence in the map does not imply a
// successful start: recheck the process on every reconciliation.
template<class Id,class Process> class SessionProcessSupervisor {
public:
    template<class Range,class Open,class Close,class Start,class Stopping>
    void reconcile(const Range& sessions,Open opened,Close closed,Start start,Stopping stopping) {
        for(auto it=children.begin();it!=children.end();) {
            if(std::find(sessions.begin(),sessions.end(),it->first)==sessions.end()) {
                it->second.reset();closed(it->first);it=children.erase(it);
            } else ++it;
        }
        for(const auto& id:sessions) {
            if(stopping()) break;
            auto it=children.find(id);
            if(it==children.end()) {
                opened(id);it=children.emplace(id,std::unique_ptr<Process>(new Process)).first;
            }
            if(!it->second->isRunning()) start(*it->second,id);
        }
    }
    void requestStopAll(){for(auto& entry:children) entry.second->requestStop();}
    void clear(){children.clear();}
private:
    std::map<Id,std::unique_ptr<Process>> children;
};
