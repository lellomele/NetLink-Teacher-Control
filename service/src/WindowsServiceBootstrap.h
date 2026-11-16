// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#pragma once
#include <windows.h>
#include <mutex>
#include "PlatformServiceFunctions.h"

// No Qt initialization, logging, configuration or plugin loading before the
// dispatcher has attached. Do not replace real milestones with a heartbeat:
// a hung initialization step must still time out in the SCM.
class WindowsServiceBootstrap final {
public:
    WindowsServiceBootstrap() {
        ready=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        release=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        if(!ready || !release) return;
        instance()=this;
        thread=CreateThread(nullptr,0,dispatch,this,0,nullptr);
        if(!thread) return;
        HANDLE events[]{ready,thread};
        connected=WaitForMultipleObjects(2,events,FALSE,INFINITE)==WAIT_OBJECT_0 && context.statusHandle && error==NO_ERROR;
        context.run=[this](const auto& entry,const auto& control) {
            { std::lock_guard<std::mutex> guard(lock); body=entry; handler=control; }
            SetEvent(release);
            WaitForSingleObject(thread,INFINITE);
            return error==NO_ERROR;
        };
    }
    ~WindowsServiceBootstrap() {
        // If core initialization fails before handoff, release ServiceMain and
        // report a failed start, never leave a live dispatcher referencing us.
        if(thread) { SetEvent(release); WaitForSingleObject(thread,INFINITE); CloseHandle(thread); }
        if(ready) CloseHandle(ready);
        if(release) CloseHandle(release);
        if(instance()==this) instance()=nullptr;
    }
    WindowsServiceBootstrap(const WindowsServiceBootstrap&)=delete;
    WindowsServiceBootstrap& operator=(const WindowsServiceBootstrap&)=delete;
    bool isConnected() const { return connected; }
    void progress() {
        std::lock_guard<std::mutex> guard(lock);
        if(connected && !body) pending();
    }
    PlatformServiceFunctions::EarlyStartup context;
private:
    static WindowsServiceBootstrap*& instance(){static WindowsServiceBootstrap* value=nullptr;return value;}
    static DWORD WINAPI dispatch(void* pointer) {
        auto& self=*static_cast<WindowsServiceBootstrap*>(pointer);
        SERVICE_TABLE_ENTRYW table[]{{const_cast<wchar_t*>(L"VeyonSchoolService"),serviceMain},{nullptr,nullptr}};
        if(!StartServiceCtrlDispatcherW(table)) self.error=GetLastError();
        return self.error;
    }
    static void WINAPI serviceMain(DWORD,LPWSTR*) {
        auto& self=*instance();
        self.context.statusHandle=RegisterServiceCtrlHandlerExW(L"VeyonSchoolService",control,&self);
        if(!self.context.statusHandle) { self.error=GetLastError(); SetEvent(self.ready); return; }
        {
            std::lock_guard<std::mutex> guard(self.lock);
            self.pending();
        }
        SetEvent(self.ready);
        WaitForSingleObject(self.release,INFINITE);
        if(self.body && !self.error) self.body();
        else {
            SERVICE_STATUS status{};status.dwServiceType=SERVICE_WIN32_OWN_PROCESS;
            status.dwCurrentState=SERVICE_STOPPED;
            status.dwWin32ExitCode=self.error?self.error:ERROR_SERVICE_NOT_ACTIVE;
            SetServiceStatus(static_cast<SERVICE_STATUS_HANDLE>(self.context.statusHandle),&status);
        }
        // A control invocation cannot outlive the WindowsServiceCore it calls.
        std::lock_guard<std::mutex> guard(self.lock);
        self.handler={};
    }
    static DWORD WINAPI control(DWORD code,DWORD type,void* data,void* pointer) {
        auto& self=*static_cast<WindowsServiceBootstrap*>(pointer);
        std::lock_guard<std::mutex> guard(self.lock);
        if(self.handler) return self.handler(code,type,data);
        if(code==SERVICE_CONTROL_INTERROGATE) return NO_ERROR;
        return ERROR_SERVICE_CANNOT_ACCEPT_CTRL;
    }
    void pending() {
        SERVICE_STATUS status{};status.dwServiceType=SERVICE_WIN32_OWN_PROCESS;
        status.dwCurrentState=SERVICE_START_PENDING;status.dwWaitHint=120000;
        status.dwCheckPoint=++context.checkpoint;
        if(!SetServiceStatus(static_cast<SERVICE_STATUS_HANDLE>(context.statusHandle),&status)) error=GetLastError();
    }
    HANDLE ready=nullptr,release=nullptr,thread=nullptr;
    DWORD error=NO_ERROR;
    bool connected=false;
    std::mutex lock;
    PlatformServiceFunctions::ServiceEntryPoint body;
    PlatformServiceFunctions::EarlyStartup::ControlHandler handler;
};
