// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#define NETLINK_MAINTENANCE_LIBRARY
#include "Maintenance.cpp"
#include <tlhelp32.h>
#include <set>
using namespace Maintenance;
bool ownedProcess(const fs::path& image,const fs::path& root) {
    if(!equal(full(image.parent_path()),full(root))) return false;
    const std::set<std::wstring> names={L"master",L"configurator",L"server",L"worker",L"cli",L"wcli",L"companion",L"service"};
    auto name=image.filename().wstring();std::transform(name.begin(),name.end(),name.begin(),::towlower);
    for(const auto& part:names) for(const auto prefix:{L"netlink-",L"veyon-"})
        if(name==std::wstring(prefix)+part+L".exe") return true;
    return false;
}
int message(const std::wstring& text,UINT flags) {
    MSGBOXPARAMSW p{};p.cbSize=sizeof(p);p.hInstance=GetModuleHandleW(nullptr);
    p.lpszText=text.c_str();p.lpszCaption=L"NetLink - Preparazione aggiornamento";
    p.dwStyle=flags|MB_USERICON;p.lpszIcon=L"netlinkicon";return MessageBoxIndirectW(&p);
}
void enableProcessAccess() {
    HANDLE token=nullptr;require(OpenProcessToken(GetCurrentProcess(),TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY,&token),"Open administrator token");
    TOKEN_PRIVILEGES privileges{};privileges.PrivilegeCount=1;
    if(!LookupPrivilegeValueW(nullptr,SE_DEBUG_NAME,&privileges.Privileges[0].Luid)) {CloseHandle(token);throw Error("Resolve process privilege");}
    privileges.Privileges[0].Attributes=SE_PRIVILEGE_ENABLED;SetLastError(ERROR_SUCCESS);
    const auto ok=AdjustTokenPrivileges(token,FALSE,&privileges,0,nullptr,nullptr);const auto error=GetLastError();CloseHandle(token);
    if(!ok || error!=ERROR_SUCCESS) throw Error("Administrator process access unavailable",error);
}
void closeProcesses(const fs::path& root) {
    // Keep the same opened handle through verification and termination: no PID reuse.
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    require(snapshot!=INVALID_HANDLE_VALUE,"Process snapshot");
    PROCESSENTRY32W entry{};entry.dwSize=sizeof(entry);
    try {for(BOOL more=Process32FirstW(snapshot,&entry);more;more=Process32NextW(snapshot,&entry)) {
        if(entry.th32ProcessID==GetCurrentProcessId()) continue;
        const std::wstring shortName=entry.szExeFile;
        if(shortName.find(L"netlink-")!=0 && shortName.find(L"veyon-")!=0) continue;
        HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|PROCESS_TERMINATE|SYNCHRONIZE,FALSE,entry.th32ProcessID);
        if(!process) {if(GetLastError()==ERROR_INVALID_PARAMETER) continue;throw Error("Cannot inspect candidate process");}
        try {
            wchar_t path[32768];DWORD length=32768;
            if(!QueryFullProcessImageNameW(process,0,path,&length)) {
                if(WaitForSingleObject(process,0)==WAIT_OBJECT_0) {CloseHandle(process);continue;}
                throw Error("Read process path");
            }
            if(ownedProcess(path,root)) {
                const auto name=fs::path(path).filename().wstring();
                // The service must have acknowledged STOP first; never kill it and
                // accidentally trigger SCM recovery during the file copy.
                require(name!=L"netlink-service.exe" && name!=L"veyon-service.exe","Service process still present; retry later");
                if(!TerminateProcess(process,0) && WaitForSingleObject(process,0)!=WAIT_OBJECT_0) throw Error("Stop NetLink process");
                require(WaitForSingleObject(process,5000)==WAIT_OBJECT_0,"Process stop timeout");
            }
        } catch(...) {CloseHandle(process);throw;}
        CloseHandle(process);
    }} catch(...) {CloseHandle(snapshot);throw;}
    CloseHandle(snapshot);
}
std::wstring blockers(const fs::path& root) {
    std::wstring result;
    for(const auto& entry:fs::recursive_directory_iterator(root)) {
        noLinks(entry.path());if(!entry.is_regular_file()) continue;
        auto ext=entry.path().extension().wstring();std::transform(ext.begin(),ext.end(),ext.begin(),::towlower);
        if(ext!=L".exe" && ext!=L".dll") continue;
        HANDLE file=CreateFileW(entry.path().c_str(),GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file==INVALID_HANDLE_VALUE) {if(result.size()<10000) result+=entry.path().filename().wstring()+L" (Windows "+std::to_wstring(GetLastError())+L")\n";}
        else CloseHandle(file);
    }
    return result;
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR command,int) {
    try {
        const std::wstring mode=command;
        if(mode==L"--self-test") {
            const fs::path root=L"C:\\NetLinkFixture";
            require(ownedProcess(root/L"netlink-worker.exe",root),"Positive ownership test");
            require(!ownedProcess(L"C:\\Other\\netlink-worker.exe",root),"Foreign process test");
            require(!ownedProcess(root/L"subdir"/L"netlink-worker.exe",root),"Subdirectory test");
            require(!ownedProcess(root/L"unrelated.exe",root),"Name test");return 0;
        }
        require(mode.empty() || mode==L"--inspect","Unknown option");
        const auto root=installRoot();require(fs::is_directory(root),"NetLink installation not found");
        struct Lease {HANDLE h=nullptr;~Lease(){if(h)CloseHandle(h);}} preparation,maintenance;
        if(mode.empty()) {
            if(message(L"Salva prima tutte le modifiche nella Console e nel Configurator.\n\nArrestare NetLink Service e chiudere forzatamente le applicazioni NetLink di tutti gli utenti di questo PC? Le sessioni remote e le funzioni attive verranno interrotte.\n\nConfigurazione, chiavi e avvio automatico non saranno modificati. Il controllo di avvio compatibile resta sospeso durante la preparazione.\n\nNon avviare contemporaneamente altri installer o applicazioni NetLink.",MB_YESNO|MB_DEFBUTTON2)!=IDYES) return ERROR_CANCELLED;
            preparation.h=CreateMutexW(nullptr,FALSE,L"Global\\NetLinkUpgradePreparation");require(preparation.h && GetLastError()!=ERROR_ALREADY_EXISTS,"Another preparation is active");
            maintenance.h=CreateMutexW(nullptr,FALSE,L"Global\\NetLinkInstallerMaintenance");require(maintenance.h && GetLastError()!=ERROR_ALREADY_EXISTS,"Installer or maintenance is active");
            enableProcessAccess();stopService(root,false);closeProcesses(root);Sleep(1000);
        }
        const auto locked=blockers(root);const bool pending=pendingRemoval(root);
        if(!locked.empty() || pending) {
            message((mode.empty()?L"Arresto eseguito, ma non e' possibile garantire l'aggiornamento senza riavvio.\n\n":L"Sola verifica: nessun servizio o processo arrestato.\n\n")+
                std::wstring(pending?L"Windows ha rimozioni pendenti: riavviare prima di installare.\n\n":L"")+locked+
                L"\nLe DLL possono essere caricate da altri programmi, che non saranno terminati. Se restano occupate, riavvia Windows. Configurazione e chiavi non sono state modificate.",MB_OK);
            return ERROR_SUCCESS_REBOOT_REQUIRED;
        }
        if(maintenance.h){CloseHandle(maintenance.h);maintenance.h=nullptr;}
        message(mode.empty()?L"Servizio e applicazioni NetLink arrestati. Nessun EXE/DLL occupato rilevato.\n\nLascia aperta questa finestra, avvia il setup e chiudila solo quando il wizard e' aperto. La riserva tra i due programmi richiede i controlli alpha.40; con quelli precedenti resta necessaria la verifica dell'installer. Configurazione e chiavi non sono state modificate. Il controllo dell'installer resta definitivo: altri processi possono riaprire i file.":L"Sola verifica: nessun EXE/DLL occupato rilevato. Nessun processo o servizio arrestato.",MB_OK);return 0;
    } catch(const Error& error) {
        const std::string what=error.what();const std::wstring text(what.begin(),what.end());
        message(L"Preparazione non completata. Non procedere presumendo che NetLink sia fermo.\n\n"+text+L"\nCodice Windows: "+std::to_wstring(error.code)+L"\n\nIl servizio potrebbe essere gia' stato arrestato. Nessuna configurazione o chiave e' stata modificata.",MB_OK);return error.code?error.code:1;
    } catch(...) {message(L"Verifica non completata. Non procedere con l'upgrade senza il controllo dell'installer. Il servizio potrebbe essere fermo; configurazione e chiavi non sono state modificate.",MB_OK);return 1;}
}
