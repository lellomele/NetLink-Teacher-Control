// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
// Compile the real control implementation against inert Win32 SCM callbacks.
// No service, registry entry, process or recovery action is created by this test.
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include <windows.h>
#include <QtTest>
#include <vector>
#include <string>
#include <cstring>
#include "../plugins/platform/windows/WindowsServiceControl.h"

namespace {
struct State {
    bool installed=false,nonCrash=false,delayed=true,ignoreStartType=false,startFails=false,badPointer=false;
    DWORD startType=SERVICE_DEMAND_START,status=SERVICE_STOPPED,reset=0,failLevel=0;
    int creates=0,changes=0,starts=0,stops=0;
    std::wstring path=L"\"C:/NetLink/netlink-service.exe\"",account=L"LocalSystem",dependencies;
    std::vector<SC_ACTION> actions;
} state;
SC_HANDLE WINAPI fakeOpenManager(LPCWSTR,LPCWSTR,DWORD) {return reinterpret_cast<SC_HANDLE>(1);}
SC_HANDLE WINAPI fakeOpenService(SC_HANDLE,LPCWSTR,DWORD) {if(state.installed) return reinterpret_cast<SC_HANDLE>(2);SetLastError(ERROR_SERVICE_DOES_NOT_EXIST);return nullptr;}
BOOL WINAPI fakeClose(SC_HANDLE) {return TRUE;}
SC_HANDLE WINAPI fakeCreate(SC_HANDLE,LPCWSTR,LPCWSTR,DWORD,DWORD,DWORD start,DWORD,LPCWSTR path,LPCWSTR,LPDWORD,LPCWSTR,LPCWSTR,LPCWSTR) {
    state.installed=true;++state.creates;state.path=path;state.startType=start;return reinterpret_cast<SC_HANDLE>(2);
}
BOOL WINAPI fakeChange(SC_HANDLE,DWORD,DWORD start,DWORD,LPCWSTR path,LPCWSTR,LPDWORD,LPCWSTR dependencies,LPCWSTR,LPCWSTR,LPCWSTR) {
    ++state.changes;if(!state.ignoreStartType && start!=SERVICE_NO_CHANGE) state.startType=start;
    if(path) state.path=path;
    if(dependencies) {state.dependencies.clear();for(auto p=dependencies;*p;p+=wcslen(p)+1) {state.dependencies+=p;state.dependencies+=L';';}}
    return TRUE;
}
BOOL WINAPI fakeQuery(SC_HANDLE,LPQUERY_SERVICE_CONFIGW config,DWORD bytes,LPDWORD needed) {
    *needed=sizeof(*config);if(bytes<*needed) {SetLastError(ERROR_INSUFFICIENT_BUFFER);return FALSE;}
    *config={};config->dwStartType=state.startType;config->dwServiceType=SERVICE_WIN32_OWN_PROCESS;
    config->lpBinaryPathName=const_cast<LPWSTR>(state.path.c_str());config->lpServiceStartName=const_cast<LPWSTR>(state.account.c_str());return TRUE;
}
BOOL WINAPI fakeChange2(SC_HANDLE,DWORD level,LPVOID value) {
    if(level==state.failLevel) {SetLastError(ERROR_ACCESS_DENIED);return FALSE;}
    if(level==SERVICE_CONFIG_FAILURE_ACTIONS) {const auto data=static_cast<SERVICE_FAILURE_ACTIONSW*>(value);state.actions.assign(data->lpsaActions,data->lpsaActions+data->cActions);state.reset=data->dwResetPeriod;}
    if(level==SERVICE_CONFIG_FAILURE_ACTIONS_FLAG) state.nonCrash=static_cast<SERVICE_FAILURE_ACTIONS_FLAG*>(value)->fFailureActionsOnNonCrashFailures;
    if(level==SERVICE_CONFIG_DELAYED_AUTO_START_INFO) state.delayed=static_cast<SERVICE_DELAYED_AUTO_START_INFO*>(value)->fDelayedAutostart;
    return TRUE;
}
BOOL WINAPI fakeQuery2(SC_HANDLE,DWORD level,LPBYTE bytes,DWORD capacity,LPDWORD needed) {
    if(level==SERVICE_CONFIG_FAILURE_ACTIONS) {
        *needed=DWORD(sizeof(SERVICE_FAILURE_ACTIONSW)+state.actions.size()*sizeof(SC_ACTION));
        if(capacity<*needed) return FALSE;
        auto data=reinterpret_cast<SERVICE_FAILURE_ACTIONSW*>(bytes);*data={};data->dwResetPeriod=state.reset;data->cActions=DWORD(state.actions.size());
        data->lpsaActions=reinterpret_cast<SC_ACTION*>(bytes+sizeof(*data));
        if(!state.actions.empty()) std::memcpy(data->lpsaActions,state.actions.data(),state.actions.size()*sizeof(SC_ACTION));
        if(state.badPointer) data->lpsaActions=reinterpret_cast<SC_ACTION*>(1);
    } else if(level==SERVICE_CONFIG_FAILURE_ACTIONS_FLAG) reinterpret_cast<SERVICE_FAILURE_ACTIONS_FLAG*>(bytes)->fFailureActionsOnNonCrashFailures=state.nonCrash;
    else if(level==SERVICE_CONFIG_DELAYED_AUTO_START_INFO) reinterpret_cast<SERVICE_DELAYED_AUTO_START_INFO*>(bytes)->fDelayedAutostart=state.delayed;
    else return FALSE;
    return TRUE;
}
BOOL WINAPI fakeStatus(SC_HANDLE,LPSERVICE_STATUS value) {*value={};value->dwCurrentState=state.status;return TRUE;}
BOOL WINAPI fakeStatusEx(SC_HANDLE,SC_STATUS_TYPE,LPBYTE bytes,DWORD,LPDWORD) {auto value=reinterpret_cast<SERVICE_STATUS_PROCESS*>(bytes);*value={};value->dwCurrentState=state.status;return TRUE;}
BOOL WINAPI fakeStart(SC_HANDLE,DWORD,LPCWSTR*) {++state.starts;if(state.startFails) {SetLastError(ERROR_SERVICE_DISABLED);return FALSE;}state.status=SERVICE_RUNNING;return TRUE;}
BOOL WINAPI fakeControl(SC_HANDLE,DWORD,LPSERVICE_STATUS value) {++state.stops;state.status=SERVICE_STOPPED;value->dwCurrentState=state.status;return TRUE;}
BOOL WINAPI fakeDelete(SC_HANDLE) {state.installed=false;return TRUE;}
}
#define OpenSCManagerW fakeOpenManager
#define OpenServiceW fakeOpenService
#define CloseServiceHandle fakeClose
#define CreateServiceW fakeCreate
#define ChangeServiceConfigW fakeChange
#define QueryServiceConfigW fakeQuery
#define ChangeServiceConfig2W fakeChange2
#define QueryServiceConfig2W fakeQuery2
#define QueryServiceStatus fakeStatus
#define QueryServiceStatusEx fakeStatusEx
#define StartServiceW fakeStart
#define ControlService fakeControl
#define DeleteService fakeDelete
#include "../plugins/platform/windows/WindowsServiceControl.cpp"

class WindowsServiceStartupTests : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void init() {state=State{};}
    void freshRegistrationVerifiesAutostartAndBoundedRecovery() {
        WindowsServiceControl service("NetLinkFixture");QVERIFY(service.install("C:/NetLink/netlink-service.exe","NetLink Fixture"));
        QCOMPARE(state.creates,1);QCOMPARE(state.startType,DWORD(SERVICE_AUTO_START));QVERIFY(!state.delayed);
        QVERIFY(state.nonCrash);QCOMPARE(state.reset,DWORD(86400));QCOMPARE(state.actions.size(),size_t(4));
        QCOMPARE(state.actions[0].Delay,DWORD(10000));QCOMPARE(state.actions[1].Delay,DWORD(30000));QCOMPARE(state.actions[2].Delay,DWORD(60000));
        QCOMPARE(state.actions.back().Type,SC_ACTION_NONE);QVERIFY(state.dependencies.find(L"LanmanWorkstation")==std::wstring::npos);
    }
    void existingRegistrationRepairsLegacyPathAndManualNativeState() {
        state.installed=true;state.path=L"\"C:/NetLink/veyon-service.exe\"";
        WindowsServiceControl service("NetLinkFixture");QVERIFY(service.install("C:/NetLink/netlink-service.exe","NetLink Fixture"));
        QCOMPARE(state.creates,0);QVERIFY(state.changes>0);QCOMPARE(QString::fromStdWString(state.path),QString("\"C:/NetLink/netlink-service.exe\""));
        QCOMPARE(state.startType,DWORD(SERVICE_AUTO_START));QVERIFY(WindowsServicePolicy::verified(reinterpret_cast<SC_HANDLE>(2)));
    }
    void refusesForeignServiceAndPropagatesRecoveryFailure() {
        state.installed=true;state.path=L"\"C:/Other/foreign.exe\"";
        WindowsServiceControl foreign("NetLinkFixture");QVERIFY(!foreign.install("C:/NetLink/netlink-service.exe","NetLink Fixture"));QCOMPARE(state.changes,0);
        state=State{};state.failLevel=SERVICE_CONFIG_FAILURE_ACTIONS_FLAG;
        WindowsServiceControl failed("NetLinkFixture");QVERIFY(!failed.install("C:/NetLink/netlink-service.exe","NetLink Fixture"));QVERIFY(state.installed);
    }
    void startTypeReadbackFailureIsNotSuccessAndManualChoiceIsSupported() {
        state.installed=true;state.ignoreStartType=true;
        WindowsServiceControl service("NetLinkFixture");QVERIFY(!service.setStartType(SERVICE_AUTO_START));
        state.ignoreStartType=false;QVERIFY(service.setStartType(SERVICE_AUTO_START));QVERIFY(service.setStartType(SERVICE_DEMAND_START));
        QCOMPARE(state.startType,DWORD(SERVICE_DEMAND_START));
    }
    void recoveryReadbackRejectsInconsistentOrUnsafeData() {
        const auto handle=reinterpret_cast<SC_HANDLE>(2);DWORD error=0;QVERIFY(WindowsServicePolicy::configure(handle,&error));QCOMPARE(error,DWORD(NO_ERROR));
        state.nonCrash=false;QVERIFY(!WindowsServicePolicy::verified(handle));state.nonCrash=true;
        state.badPointer=true;QVERIFY(!WindowsServicePolicy::verified(handle));state.badPointer=false;
        state.actions[0].Type=SC_ACTION_REBOOT;QVERIFY(!WindowsServicePolicy::verified(handle));
    }
    void nativeStartStopAndFailuresAreNotMaskedByPresence() {
        state.installed=true;WindowsServiceControl service("NetLinkFixture");
        state.startFails=true;QVERIFY(!service.start());QVERIFY(service.isRegistered());QVERIFY(!service.isRunning());
        state.startFails=false;QVERIFY(service.start());QVERIFY(service.start());QCOMPARE(state.starts,2);
        QVERIFY(service.stop());QVERIFY(service.stop());QCOMPARE(state.stops,1);
    }
    void stopAndInitializationFailureHaveDifferentScmMeaning() {
        QCOMPARE(WindowsServicePolicy::stoppedExit(true,ERROR_INVALID_HANDLE),DWORD(NO_ERROR));
        QCOMPARE(WindowsServicePolicy::stoppedExit(false,ERROR_INVALID_HANDLE),DWORD(ERROR_INVALID_HANDLE));
        QVERIFY(WindowsServicePolicy::stoppedExit(false,NO_ERROR)!=NO_ERROR);
        QCOMPARE(WindowsServicePolicy::acceptedControls(SERVICE_START_PENDING),DWORD(0));
        QCOMPARE(WindowsServicePolicy::acceptedControls(SERVICE_STOP_PENDING),DWORD(0));
        QCOMPARE(WindowsServicePolicy::acceptedControls(SERVICE_STOPPED),DWORD(0));
        QVERIFY(WindowsServicePolicy::acceptedControls(SERVICE_RUNNING)&SERVICE_ACCEPT_STOP);
    }
};
QTEST_GUILESS_MAIN(WindowsServiceStartupTests)
#include "WindowsServiceStartupTests.moc"
