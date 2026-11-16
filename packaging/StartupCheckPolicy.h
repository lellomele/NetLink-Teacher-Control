// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <string>
namespace StartupCheckPolicy {
constexpr unsigned Attempts=3, RetryMilliseconds=120000, PendingMilliseconds=120000;
inline int delay(const std::wstring& value) {
    return value==L"3"?3:value==L"5"?5:value==L"off"?0:-1;
}
enum class Action { Skip, Done, Wait, Start };
inline Action action(unsigned startType,unsigned state,bool maintenance,bool cancelled) {
    if(maintenance || cancelled || startType!=2) return Action::Skip;
    if(state==4) return Action::Done;
    if(state==2 || state==3) return Action::Wait;
    return state==1?Action::Start:Action::Skip;
}
}
