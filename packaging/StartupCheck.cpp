// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
// Native, statically linked: can recover a service before Qt is usable.
#define NETLINK_MAINTENANCE_LIBRARY
#include "Maintenance.cpp"
#include "StartupCheckPolicy.h"
#include <taskschd.h>
#include <memory>

namespace StartupCheck {
using namespace Maintenance;
constexpr auto TaskName=L"NetLink Service Startup Check";
constexpr auto Preference=L"SOFTWARE\\NetLink\\StartupCheck";
constexpr auto StopName=L"Global\\NetLinkStartupCheckStop";
constexpr auto RunName=L"Global\\NetLinkStartupCheckRunning";
constexpr auto MaintenanceName=L"Global\\NetLinkInstallerMaintenance";
constexpr auto ConfigurationName=L"Global\\NetLinkStartupCheckConfiguration";
struct Handle {HANDLE value=nullptr;~Handle(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);}};
template<class T> struct Com {
    T* p=nullptr; ~Com(){if(p)p->Release();}
    Com()=default;Com(const Com&)=delete;Com& operator=(const Com&)=delete;
    T** out(){return &p;} T* operator->()const{return p;}
};
struct Text {
    BSTR value; explicit Text(const std::wstring& s):value(SysAllocString(s.c_str())) {require(value,"Allocate task text");}
    ~Text(){SysFreeString(value);} operator BSTR()const{return value;}
};
struct Variant:VARIANT {
    Variant(){VariantInit(this);} explicit Variant(const wchar_t* text):Variant(){vt=VT_BSTR;bstrVal=SysAllocString(text);require(bstrVal,"Allocate task parameter");}
    ~Variant(){VariantClear(this);}
};
void check(HRESULT code,const char* action){if(FAILED(code))throw Error(action,DWORD(code));}
struct Scheduler {
    Com<ITaskService> service;Com<ITaskFolder> folder;
    Scheduler(){
        check(CoCreateInstance(CLSID_TaskScheduler,nullptr,CLSCTX_INPROC_SERVER,IID_ITaskService,reinterpret_cast<void**>(service.out())),"Open Task Scheduler");
        Variant empty;check(service->Connect(empty,empty,empty,empty),"Connect Task Scheduler");
        Text root(L"\\");check(service->GetFolder(root,folder.out()),"Open task folder");
    }
};
std::wstring executable(){wchar_t path[32768];auto n=GetModuleFileNameW(nullptr,path,32768);require(n && n<32768,"Read helper location");return full(path);}
bool approvedHelper(const fs::path& path) {
    auto pf=known(FOLDERID_ProgramFilesX64);
    if(equal(full(path),full(pf/L"Veyon School"/L"netlink-startup-check.exe")) ||
       equal(full(path),full(pf/L"NetLink Service Tools"/L"netlink-startup-check.exe")))return true;
    const auto folder=path.parent_path().filename().wstring();
    // Portable utility revisions are immutable, hash-addressed files. Never
    // overwrite a helper which an older task may still have mapped in memory.
    return equal(path.filename().wstring(),L"netlink-startup-check.exe") &&
        equal(full(path.parent_path().parent_path()),full(pf/L"NetLink Service Tools")) && folder.size()==64 &&
        std::all_of(folder.begin(),folder.end(),[](wchar_t c){return (c>=L'0'&&c<=L'9')||(c>=L'a'&&c<=L'f')||(c>=L'A'&&c<=L'F');});
}
void protectedPath(const fs::path& path) {
    noLinks(path);
    // In addition to fixed paths reject permissive ACLs introduced by hand or
    // an old installer. A SYSTEM task must never execute a user-writable image.
    const auto pf=known(FOLDERID_ProgramFilesX64);
    require(within(path,pf),"Task executable must be in Program Files");
    auto current=path;
    while(within(current,pf)) {
        PACL acl=nullptr;PSID owner=nullptr;PSECURITY_DESCRIPTOR descriptor=nullptr;
        auto rc=GetNamedSecurityInfoW(const_cast<wchar_t*>(current.c_str()),SE_FILE_OBJECT,DACL_SECURITY_INFORMATION|OWNER_SECURITY_INFORMATION,&owner,nullptr,&acl,nullptr,&descriptor);
        if(rc!=ERROR_SUCCESS)throw Error("Read executable ACL",rc);
        bool safe=acl!=nullptr;
        BYTE admin[SECURITY_MAX_SID_SIZE],system[SECURITY_MAX_SID_SIZE];DWORD a=sizeof(admin),s=sizeof(system);
        CreateWellKnownSid(WinBuiltinAdministratorsSid,nullptr,admin,&a);CreateWellKnownSid(WinLocalSystemSid,nullptr,system,&s);
        PSID trusted=nullptr;ConvertStringSidToSidW(L"S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464",&trusted);
        safe=safe && owner && (EqualSid(owner,admin)||EqualSid(owner,system)||(trusted&&EqualSid(owner,trusted)));
        for(DWORD i=0;safe && i<acl->AceCount;++i) {
            void* raw=nullptr;if(!GetAce(acl,i,&raw)){safe=false;break;}
            auto ace=static_cast<ACCESS_ALLOWED_ACE*>(raw);
            if(ace->Header.AceFlags&INHERIT_ONLY_ACE)continue;
            if(ace->Header.AceType==ACCESS_ALLOWED_ACE_TYPE &&
               (ace->Mask&(GENERIC_ALL|GENERIC_WRITE|FILE_WRITE_DATA|FILE_APPEND_DATA|FILE_DELETE_CHILD|FILE_WRITE_EA|FILE_WRITE_ATTRIBUTES|DELETE|WRITE_DAC|WRITE_OWNER))) {
                auto sid=&ace->SidStart;
                safe=EqualSid(sid,admin)||EqualSid(sid,system)||(trusted&&EqualSid(sid,trusted));
            } else if(ace->Header.AceType!=ACCESS_ALLOWED_ACE_TYPE && ace->Header.AceType!=ACCESS_DENIED_ACE_TYPE) safe=false;
        }
        if(trusted)LocalFree(trusted);LocalFree(descriptor);
        require(safe,"User-writable executable or parent refused");
        if(equal(full(current),full(pf)))break;
        current=current.parent_path();
    }
}
bool existingOwned(Scheduler& scheduler,Com<IRegisteredTask>& task) {
    Text name(TaskName);auto rc=scheduler.folder->GetTask(name,task.out());
    if(rc==HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))return false;
    check(rc,"Read existing startup task");
    Com<ITaskDefinition> definition;check(task->get_Definition(definition.out()),"Read task definition");
    Com<IRegistrationInfo> info;check(definition->get_RegistrationInfo(info.out()),"Read task owner");
    // Windows normalizes RegistrationInfo.URI to the registered task path.
    // Source/description plus the exact action identify our task instead.
    BSTR source=nullptr,description=nullptr;check(info->get_Source(&source),"Read task source");check(info->get_Description(&description),"Read task description");
    bool owned=(source&&std::wstring(source)==L"NetLink startup-check v1") ||
        (description&&std::wstring(description)==L"Checks NetLink after boot; retries only an automatic service. Does not change keys or configuration.");
    SysFreeString(source);SysFreeString(description);
    Com<IActionCollection> actions;check(definition->get_Actions(actions.out()),"Read task actions");long count=0;check(actions->get_Count(&count),"Read action count");
    Com<IAction> action;Com<IExecAction> exec;check(actions->get_Item(1,action.out()),"Read task action");
    check(action->QueryInterface(IID_IExecAction,reinterpret_cast<void**>(exec.out())),"Read executable action");
    BSTR path=nullptr;check(exec->get_Path(&path),"Read task executable");
    owned=owned && count==1 && path && approvedHelper(path);SysFreeString(path);
    require(owned,"A different task uses the NetLink task name");return true;
}
int effectiveDelay(IRegisteredTask* task) {
    VARIANT_BOOL enabled=VARIANT_FALSE;check(task->get_Enabled(&enabled),"Read task enabled state");require(enabled==VARIANT_TRUE,"Startup task is disabled");
    Com<ITaskDefinition> definition;check(task->get_Definition(definition.out()),"Read task definition");
    Com<IPrincipal> principal;check(definition->get_Principal(principal.out()),"Read task principal");
    BSTR user=nullptr;check(principal->get_UserId(&user),"Read task user");bool system=user&&(equal(user,L"SYSTEM")||equal(user,L"S-1-5-18")||equal(user,L"NT AUTHORITY\\SYSTEM"));SysFreeString(user);require(system,"Startup task is not SYSTEM");
    TASK_LOGON_TYPE logon;check(principal->get_LogonType(&logon),"Read task logon type");require(logon==TASK_LOGON_SERVICE_ACCOUNT,"Unexpected task logon type");
    Com<ITriggerCollection> triggers;check(definition->get_Triggers(triggers.out()),"Read task triggers");long count=0;check(triggers->get_Count(&count),"Read trigger count");require(count==1,"Unexpected startup triggers");
    Com<ITrigger> trigger;check(triggers->get_Item(1,trigger.out()),"Read boot trigger");Com<IBootTrigger> boot;
    check(trigger->QueryInterface(IID_IBootTrigger,reinterpret_cast<void**>(boot.out())),"Task is not a boot check");
    check(trigger->get_Enabled(&enabled),"Read trigger state");require(enabled==VARIANT_TRUE,"Boot trigger is disabled");
    BSTR value=nullptr;check(boot->get_Delay(&value),"Read effective boot delay");int minutes=value&&std::wstring(value)==L"PT3M"?3:value&&std::wstring(value)==L"PT5M"?5:-1;SysFreeString(value);require(minutes>=0,"Unexpected effective boot delay");
    Com<IActionCollection> actions;check(definition->get_Actions(actions.out()),"Read effective actions");Com<IAction> action;check(actions->get_Item(1,action.out()),"Read effective action");Com<IExecAction> exec;check(action->QueryInterface(IID_IExecAction,reinterpret_cast<void**>(exec.out())),"Read effective executable");
    BSTR arguments=nullptr;check(exec->get_Arguments(&arguments),"Read task arguments");bool correct=arguments&&std::wstring(arguments)==L"--check";SysFreeString(arguments);require(correct,"Unexpected startup arguments");
    return minutes;
}
std::wstring escape(const std::wstring& text){std::wstring out;for(auto c:text){if(c==L'&')out+=L"&amp;";else if(c==L'<')out+=L"&lt;";else if(c==L'>')out+=L"&gt;";else if(c==L'\"')out+=L"&quot;";else out+=c;}return out;}
int preference(){DWORD value=3,size=sizeof(value);auto rc=RegGetValueW(HKEY_LOCAL_MACHINE,Preference,L"DelayMinutes",RRF_RT_REG_DWORD,nullptr,&value,&size);if(rc==ERROR_FILE_NOT_FOUND)return 3;if(rc!=ERROR_SUCCESS)throw Error("Read startup preference",rc);require(value==0||value==3||value==5,"Invalid startup preference");return int(value);}
void savePreference(int delay){HKEY key=nullptr;auto rc=RegCreateKeyExW(HKEY_LOCAL_MACHINE,Preference,0,nullptr,0,KEY_SET_VALUE|KEY_WOW64_64KEY,nullptr,&key,nullptr);if(rc!=ERROR_SUCCESS)throw Error("Write startup preference",rc);DWORD value=DWORD(delay);rc=RegSetValueExW(key,L"DelayMinutes",0,REG_DWORD,reinterpret_cast<BYTE*>(&value),sizeof(value));RegCloseKey(key);if(rc!=ERROR_SUCCESS)throw Error("Write startup delay",rc);}
void stopRunning(){auto event=OpenEventW(EVENT_MODIFY_STATE,FALSE,StopName);if(event){SetEvent(event);CloseHandle(event);}else if(GetLastError()!=ERROR_FILE_NOT_FOUND)throw Error("Signal startup check");
    auto deadline=GetTickCount64()+35000;
    do{auto running=OpenMutexW(SYNCHRONIZE,FALSE,RunName);if(!running){require(GetLastError()==ERROR_FILE_NOT_FOUND,"Inspect running startup check");return;}CloseHandle(running);Sleep(100);}while(GetTickCount64()<deadline);
    throw Error("Startup check is still busy; retry before replacing files",ERROR_BUSY);
}
void configure(int minutes,bool persist=true) {
    require(minutes==0||minutes==3||minutes==5,"Invalid delay");
    // The lease belongs to the native operation, not only its GUI parent. If
    // the parent times out, an installer still cannot race a late COM write.
    PSECURITY_DESCRIPTOR descriptor=nullptr;require(ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:P(A;;GA;;;SY)(A;;GA;;;BA)",SDDL_REVISION_1,&descriptor,nullptr),"Configuration security");
    SECURITY_ATTRIBUTES attributes{sizeof(attributes),descriptor,FALSE};Handle lease;
    lease.value=CreateMutexW(&attributes,FALSE,ConfigurationName);auto leaseError=GetLastError();LocalFree(descriptor);
    require(lease.value,"Create configuration lease");if(leaseError==ERROR_ALREADY_EXISTS)throw Error("Another startup configuration is still in progress",ERROR_BUSY);
    Scheduler scheduler;Com<IRegisteredTask> previous;
    bool found=existingOwned(scheduler,previous);
    stopRunning();
    if(minutes==0){if(found){Text name(TaskName);check(scheduler.folder->DeleteTask(name,0),"Remove startup task");}if(persist)savePreference(0);return;}
    auto path=executable();require(approvedHelper(path),"Unprotected helper location refused");protectedPath(path);
    const std::wstring xml=L"<?xml version=\"1.0\"?><Task version=\"1.2\" xmlns=\"http://schemas.microsoft.com/windows/2004/02/mit/task\">"
      L"<RegistrationInfo><Description>Checks NetLink after boot; retries only an automatic service. Does not change keys or configuration.</Description><Source>NetLink startup-check v1</Source></RegistrationInfo>"
      L"<Triggers><BootTrigger><Enabled>true</Enabled><Delay>PT"+std::to_wstring(minutes)+L"M</Delay></BootTrigger></Triggers>"
      // ServiceAccount is a COM TASK_LOGON_TYPE value, not a valid XML
      // logonType enumeration. Supply it to RegisterTask, not in the XML.
      L"<Principals><Principal id=\"System\"><UserId>S-1-5-18</UserId><RunLevel>HighestAvailable</RunLevel></Principal></Principals>"
      L"<Settings><MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy><DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries><StopIfGoingOnBatteries>false</StopIfGoingOnBatteries><StartWhenAvailable>true</StartWhenAvailable><ExecutionTimeLimit>PT15M</ExecutionTimeLimit><Priority>7</Priority></Settings>"
      L"<Actions Context=\"System\"><Exec><Command>"+escape(path)+L"</Command><Arguments>--check</Arguments></Exec></Actions></Task>";
    Text name(TaskName),definition(xml);Variant user(L"SYSTEM"),empty,security(L"D:P(A;;FA;;;SY)(A;;FA;;;BA)");Com<IRegisteredTask> task;
    check(scheduler.folder->RegisterTask(name,definition,TASK_CREATE_OR_UPDATE,user,empty,TASK_LOGON_SERVICE_ACCOUNT,security,task.out()),"Register boot check");
    // Read the effective task back before persisting the selected delay.
    require(effectiveDelay(task.p)==minutes,"Startup task verification failed");
    if(persist)savePreference(minutes);
}
bool maintenance(){auto handle=OpenMutexW(SYNCHRONIZE,FALSE,MaintenanceName);if(handle){CloseHandle(handle);return true;}auto error=GetLastError();if(error!=ERROR_FILE_NOT_FOUND)throw Error("Read maintenance state",error);return false;}
void quiesce(){
    stopRunning();const auto deadline=GetTickCount64()+35000;
    do {auto lease=OpenMutexW(SYNCHRONIZE,FALSE,ConfigurationName);
        if(!lease){require(GetLastError()==ERROR_FILE_NOT_FOUND,"Read startup configuration lease");return;}
        CloseHandle(lease);Sleep(100);
    }while(GetTickCount64()<deadline);
    throw Error("Startup configuration is still busy; no files should be replaced",ERROR_BUSY);
}
struct Service {SC_HANDLE value=nullptr;~Service(){if(value)CloseServiceHandle(value);}};
struct Log {
    Handle file;
    Log(){auto root=known(FOLDERID_ProgramData)/L"NetLink";protectDirectory(root);root/=L"StartupCheck";protectDirectory(root);GUID id{};check(CoCreateGuid(&id),"Log identifier");wchar_t name[40];StringFromGUID2(id,name,40);
        file.value=CreateFileW((root/(std::wstring(name)+L".log")).c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);require(file.value!=INVALID_HANDLE_VALUE,"Create startup log");}
    void write(const std::string& text){SYSTEMTIME t{};GetSystemTime(&t);char prefix[50];snprintf(prefix,sizeof(prefix),"%04u-%02u-%02uT%02u:%02u:%02uZ ",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond);auto line=std::string(prefix)+text+"\r\n";DWORD written=0;require(WriteFile(file.value,line.data(),DWORD(line.size()),&written,nullptr)&&written==line.size(),"Write startup log");FlushFileBuffers(file.value);std::cout<<line;}
};
unsigned startType(SC_HANDLE service) {
    DWORD size=0;QueryServiceConfigW(service,nullptr,0,&size);require(size>=sizeof(QUERY_SERVICE_CONFIGW)&&size<65536,"Read service size");std::vector<BYTE> buffer(size);
    require(QueryServiceConfigW(service,reinterpret_cast<QUERY_SERVICE_CONFIGW*>(buffer.data()),size,&size),"Read service configuration");
    auto config=reinterpret_cast<QUERY_SERVICE_CONFIGW*>(buffer.data());auto root=installRoot();
    require(config->dwServiceType==SERVICE_WIN32_OWN_PROCESS && config->lpServiceStartName && equal(config->lpServiceStartName,L"LocalSystem"),"Unexpected NetLink service identity");
    const auto expected=(root/L"netlink-service.exe").wstring();
    require(config->lpBinaryPathName && (equal(config->lpBinaryPathName,L"\""+expected+L"\"")||equal(config->lpBinaryPathName,expected)),"Unexpected NetLink service executable");
    protectedPath(expected);return config->dwStartType;
}
int run() {
    // Secure named objects prevent a standard user from injecting a controller.
    PSECURITY_DESCRIPTOR descriptor=nullptr;require(ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:P(A;;GA;;;SY)(A;;GA;;;BA)",SDDL_REVISION_1,&descriptor,nullptr),"Check security");
    SECURITY_ATTRIBUTES attributes{sizeof(attributes),descriptor,FALSE};Handle running,stop;
    running.value=CreateMutexW(&attributes,TRUE,RunName);auto error=GetLastError();
    if(!running.value||error==ERROR_ALREADY_EXISTS){LocalFree(descriptor);return running.value?0:ERROR_ACCESS_DENIED;}
    stop.value=CreateEventW(&attributes,TRUE,FALSE,StopName);error=GetLastError();LocalFree(descriptor);require(stop.value,"Create check cancellation event");require(error!=ERROR_ALREADY_EXISTS,"Unexpected cancellation object");
    Log log;log.write("NetLink startup check; at most 3 start attempts; pause 120 seconds.");
    if(maintenance()){log.write("Skipped: installer/uninstaller is active.");return 0;}
    Service manager;manager.value=OpenSCManagerW(nullptr,nullptr,SC_MANAGER_CONNECT);require(manager.value,"Open service manager");
    Service service;service.value=OpenServiceW(manager.value,L"VeyonSchoolService",SERVICE_QUERY_CONFIG|SERVICE_QUERY_STATUS|SERVICE_START);
    if(!service.value){error=GetLastError();log.write("Service unavailable; Windows "+std::to_string(error));return error==ERROR_SERVICE_DOES_NOT_EXIST||error==ERROR_SERVICE_MARKED_FOR_DELETE?0:int(error);}
    unsigned attempts=0;auto pendingDeadline=GetTickCount64()+StartupCheckPolicy::PendingMilliseconds;
    for(;;){
        SERVICE_STATUS status{};require(QueryServiceStatus(service.value,&status),"Read service status");
        auto type=startType(service.value);
        auto action=StartupCheckPolicy::action(type,status.dwCurrentState,maintenance(),WaitForSingleObject(stop.value,0)==WAIT_OBJECT_0);
        if(action==StartupCheckPolicy::Action::Skip){log.write("Skipped/cancelled: service type "+std::to_string(type)+", state "+std::to_string(status.dwCurrentState));return 0;}
        if(action==StartupCheckPolicy::Action::Done){log.write("Service running; no restart requested.");return 0;}
        if(action==StartupCheckPolicy::Action::Wait){
            if(GetTickCount64()>=pendingDeadline){log.write("Service remains pending after 120 seconds; not killed or duplicated.");return ERROR_TIMEOUT;}
            if(WaitForSingleObject(stop.value,1000)==WAIT_FAILED)throw Error("Wait for pending service");continue;
        }
        if(attempts>=StartupCheckPolicy::Attempts){log.write("Stopped: all 3 attempts failed; inspect SCM events and service logs.");return ERROR_SERVICE_NOT_ACTIVE;}
        ++attempts;log.write("Start request "+std::to_string(attempts));
        // Recheck immediately before the only mutating SCM call. A concurrently
        // running installer also waits for our process to exit before stopping
        // or replacing anything, closing the maintenance race in both directions.
        if(maintenance()||WaitForSingleObject(stop.value,0)==WAIT_OBJECT_0){log.write("Cancelled before start request.");return 0;}
        if(!StartServiceW(service.value,0,nullptr)) {
            error=GetLastError();log.write("StartService failed; Windows "+std::to_string(error));
            if(error==ERROR_SERVICE_DISABLED||error==ERROR_SERVICE_MARKED_FOR_DELETE)return int(error);
            if(error!=ERROR_SERVICE_ALREADY_RUNNING) {
                if(WaitForSingleObject(stop.value,StartupCheckPolicy::RetryMilliseconds)==WAIT_OBJECT_0){log.write("Cancelled during retry pause.");return 0;}
                pendingDeadline=GetTickCount64()+StartupCheckPolicy::PendingMilliseconds;continue;
            }
        }
        pendingDeadline=GetTickCount64()+StartupCheckPolicy::PendingMilliseconds;
        // A successful StartService can still be followed by immediate failure.
        if(WaitForSingleObject(stop.value,1000)==WAIT_OBJECT_0){log.write("Cancelled.");return 0;}
        require(QueryServiceStatus(service.value,&status),"Read start outcome");
        if(status.dwCurrentState==SERVICE_STOPPED && WaitForSingleObject(stop.value,StartupCheckPolicy::RetryMilliseconds)==WAIT_OBJECT_0)return 0;
    }
}
}

#ifndef NETLINK_STARTUP_CHECK_LIBRARY
int wmain(int argc,wchar_t** argv) {
    using namespace StartupCheck;
    auto initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);if(FAILED(initialized))return int(initialized);
    int result=0;
    try {
        require(argc>=2,"Expected --check, --configure 3|5|off, --remove, or --status");
        const std::wstring action=argv[1];
        if(action==L"--check"&&argc==2)result=run();
        else if(action==L"--configure"&&argc==3){int minutes=StartupCheckPolicy::delay(argv[2]);require(minutes>=0,"Use 3, 5, or off");configure(minutes);}
        else if(action==L"--remove"&&argc==2)configure(0,false);
        else if(action==L"--quiesce"&&argc==2)quiesce();
        else if(action==L"--status"&&argc==2){Scheduler scheduler;Com<IRegisteredTask> task;bool present=existingOwned(scheduler,task);int selected=preference();if(present)require(effectiveDelay(task.p)==selected,"Task delay differs from the saved preference; configure it again");std::cout<<"delay="<<selected<<"; task="<<(present?"present":"absent")<<"\n";}
        else throw Error("Invalid startup-check arguments",ERROR_INVALID_PARAMETER);
    }catch(const Error& error){std::cerr<<error.what()<<" (Windows "<<error.code<<")\n";result=error.code?int(error.code):ERROR_GEN_FAILURE;
        if(argc>=2 && std::wstring(argv[1])!=L"--status")try{Log log;log.write(std::string(error.what())+"; Windows "+std::to_string(error.code));}catch(...){}
    }catch(const std::exception& error){std::cerr<<error.what()<<"\n";result=ERROR_GEN_FAILURE;}
    CoUninitialize();return result;
}
#endif
