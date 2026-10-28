// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#pragma once
#include <windows.h>
#include <QVariantMap>
#include <QStringList>
#include <array>
#include <functional>

// Native SCM policy only. No external watchdog, scheduled task or reboot action.
// Callbacks allow exhaustive tests without touching any installed service.
namespace WindowsServicePolicy {
static constexpr DWORD ResetPeriod=86400;
inline std::array<SC_ACTION,4> actions() {
    return {{{SC_ACTION_RESTART,10000},{SC_ACTION_RESTART,30000},{SC_ACTION_RESTART,60000},{SC_ACTION_NONE,0}}};
}
struct Api {
    std::function<BOOL(SC_HANDLE,DWORD,LPVOID)> change=ChangeServiceConfig2W;
    std::function<BOOL(SC_HANDLE,DWORD,LPBYTE,DWORD,LPDWORD)> query=QueryServiceConfig2W;
};
inline bool verified(SC_HANDLE service,const Api& api={}) {
    alignas(SERVICE_FAILURE_ACTIONSW) std::array<BYTE,8192> bytes{};DWORD size=0;
    if(!api.query(service,SERVICE_CONFIG_FAILURE_ACTIONS,bytes.data(),DWORD(bytes.size()),&size)) return false;
    const auto config=reinterpret_cast<const SERVICE_FAILURE_ACTIONSW*>(bytes.data());
    const auto expected=actions();
    if(config->dwResetPeriod!=ResetPeriod || config->cActions!=expected.size() || !config->lpsaActions) return false;
    const auto begin=reinterpret_cast<quintptr>(bytes.data()),end=begin+bytes.size();
    const auto pointer=reinterpret_cast<quintptr>(config->lpsaActions);
    if(pointer<begin || pointer>end-sizeof(SC_ACTION)*expected.size()) return false;
    for(size_t i=0;i<expected.size();++i)
        if(config->lpsaActions[i].Type!=expected[i].Type || config->lpsaActions[i].Delay!=expected[i].Delay) return false;
    SERVICE_FAILURE_ACTIONS_FLAG flag{};
    return api.query(service,SERVICE_CONFIG_FAILURE_ACTIONS_FLAG,reinterpret_cast<LPBYTE>(&flag),sizeof(flag),&size) && flag.fFailureActionsOnNonCrashFailures;
}
inline bool configure(SC_HANDLE service,DWORD* error=nullptr,const Api& api={}) {
    auto retries=actions();wchar_t empty[]=L"";
    SERVICE_FAILURE_ACTIONSW config{};config.dwResetPeriod=ResetPeriod;config.cActions=DWORD(retries.size());
    config.lpsaActions=retries.data();config.lpCommand=empty;config.lpRebootMsg=empty;
    SERVICE_FAILURE_ACTIONS_FLAG flag{TRUE};
    if(!api.change(service,SERVICE_CONFIG_FAILURE_ACTIONS,&config) ||
       !api.change(service,SERVICE_CONFIG_FAILURE_ACTIONS_FLAG,&flag)) {
        if(error) *error=GetLastError();
        return false;
    }
    if(!verified(service,api)) {if(error) *error=ERROR_INVALID_DATA;return false;}
    if(error) *error=NO_ERROR;
    return true;
}
inline DWORD stoppedExit(bool stopRequested,DWORD failure) {
    return stopRequested?NO_ERROR:failure?failure:ERROR_PROCESS_ABORTED;
}
inline DWORD acceptedControls(DWORD state) {
    return state==SERVICE_RUNNING?SERVICE_ACCEPT_STOP|SERVICE_ACCEPT_SHUTDOWN|SERVICE_ACCEPT_SESSIONCHANGE:0;
}
inline QVariantMap details(SC_HANDLE service) {
    QVariantMap result;SERVICE_STATUS_PROCESS status{};DWORD size=0;
    if(QueryServiceStatusEx(service,SC_STATUS_PROCESS_INFO,reinterpret_cast<LPBYTE>(&status),sizeof(status),&size)) {
        result.insert(QStringLiteral("state"),status.dwCurrentState==SERVICE_RUNNING?QStringLiteral("running"):status.dwCurrentState==SERVICE_STOPPED?QStringLiteral("stopped"):QStringLiteral("transition"));
        result.insert(QStringLiteral("pid"),quint32(status.dwProcessId));
        result.insert(QStringLiteral("win32ExitCode"),quint32(status.dwWin32ExitCode));
        result.insert(QStringLiteral("serviceSpecificExitCode"),quint32(status.dwServiceSpecificExitCode));
    } else result.insert(QStringLiteral("queryError"),quint32(GetLastError()));
    alignas(QUERY_SERVICE_CONFIGW) std::array<BYTE,8192> bytes{};
    if(QueryServiceConfigW(service,reinterpret_cast<QUERY_SERVICE_CONFIGW*>(bytes.data()),DWORD(bytes.size()),&size)) {
        const auto config=reinterpret_cast<const QUERY_SERVICE_CONFIGW*>(bytes.data());
        result.insert(QStringLiteral("imagePath"),QString::fromWCharArray(config->lpBinaryPathName));
        result.insert(QStringLiteral("account"),QString::fromWCharArray(config->lpServiceStartName));
        result.insert(QStringLiteral("startType"),quint32(config->dwStartType));
        QStringList dependencies;
        for(const auto* item=config->lpDependencies;item && *item;item+=wcslen(item)+1) dependencies.append(QString::fromWCharArray(item));
        result.insert(QStringLiteral("dependencies"),dependencies);
    } else result.insert(QStringLiteral("queryError"),quint32(GetLastError()));
    SERVICE_DELAYED_AUTO_START_INFO delay{};
    if(QueryServiceConfig2W(service,SERVICE_CONFIG_DELAYED_AUTO_START_INFO,reinterpret_cast<LPBYTE>(&delay),sizeof(delay),&size))
        result.insert(QStringLiteral("delayedAutoStart"),bool(delay.fDelayedAutostart));
    SERVICE_FAILURE_ACTIONS_FLAG flag{};
    if(QueryServiceConfig2W(service,SERVICE_CONFIG_FAILURE_ACTIONS_FLAG,reinterpret_cast<LPBYTE>(&flag),sizeof(flag),&size))
        result.insert(QStringLiteral("failureActionsOnErrors"),bool(flag.fFailureActionsOnNonCrashFailures));
    result.insert(QStringLiteral("recoveryPolicyVerified"),verified(service));
    return result;
}
}
