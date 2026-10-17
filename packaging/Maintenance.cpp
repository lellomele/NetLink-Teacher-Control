// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
// Deliberately independent of Qt and all DLLs shipped by NetLink.
#include <windows.h>
#include <shlobj.h>
#include <sddl.h>
#include <aclapi.h>
#include <netfw.h>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace Maintenance {
namespace fs = std::filesystem;
const wchar_t* configKey = L"SOFTWARE\\Veyon Solutions\\Veyon";
const wchar_t* appearanceKey = L"Software\\Veyon\\NetLinkAppearance";

struct Error : std::runtime_error {
    DWORD code;
    Error(const char* action, DWORD value = GetLastError()) : std::runtime_error(action), code(value) {}
};
void require(bool result, const char* action) { if(!result) throw Error(action); }
std::wstring full(const fs::path& path) {
    wchar_t out[32768];
    const auto size = GetFullPathNameW(path.c_str(), 32768, out, nullptr);
    require(size && size < 32768, "Invalid absolute path");
    auto value = std::wstring(out);
    while(value.size() > 3 && value.back() == L'\\') value.pop_back();
    return value;
}
bool equal(const std::wstring& a, const std::wstring& b) { return _wcsicmp(a.c_str(), b.c_str()) == 0; }
bool within(const fs::path& path, const fs::path& root) {
    const auto p = full(path), r = full(root);
    return equal(p, r) || (p.size() > r.size() && p[r.size()] == L'\\' &&
        _wcsnicmp(p.c_str(), r.c_str(), r.size()) == 0);
}
fs::path known(const KNOWNFOLDERID& id) {
    PWSTR value = nullptr;
    require(SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &value)), "Known folder unavailable");
    const fs::path result(value); CoTaskMemFree(value); return result;
}
void noLinks(const fs::path& path) {
    // Reject a redirected install/recovery root, including any existing ancestor.
    auto current = fs::path(full(path));
    while(!current.empty() && current != current.root_path()) {
        const auto attributes = GetFileAttributesW(current.c_str());
        if(attributes == INVALID_FILE_ATTRIBUTES) {
            const auto error = GetLastError();
            if(error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) throw Error("Cannot inspect path", error);
        } else require(!(attributes & FILE_ATTRIBUTE_REPARSE_POINT), "Redirected root refused");
        current = current.parent_path();
    }
}
fs::path installRoot() { auto p = known(FOLDERID_ProgramFilesX64) / L"Veyon School"; noLinks(p); return p; }
void protectObject(const fs::path& path, bool directory) {
    noLinks(path);
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    require(ConvertStringSecurityDescriptorToSecurityDescriptorW(
        L"O:BAD:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)", SDDL_REVISION_1, &descriptor, nullptr), "Recovery ACL");
    SECURITY_ATTRIBUTES attributes{sizeof(attributes), descriptor, FALSE};
    const bool created = !directory || CreateDirectoryW(path.c_str(), &attributes);
    const auto error = created ? ERROR_SUCCESS : GetLastError();
    if(error != ERROR_SUCCESS && error != ERROR_ALREADY_EXISTS) { LocalFree(descriptor); throw Error("Create recovery directory", error); }
    BOOL present = FALSE, defaulted = FALSE; PACL dacl = nullptr;
    GetSecurityDescriptorDacl(descriptor, &present, &dacl, &defaulted);
    PSID owner=nullptr; GetSecurityDescriptorOwner(descriptor,&owner,&defaulted);
    const auto result = SetNamedSecurityInfoW(const_cast<wchar_t*>(path.c_str()), SE_FILE_OBJECT,
        OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, owner, nullptr, dacl, nullptr);
    LocalFree(descriptor);
    if(result!=ERROR_SUCCESS) throw Error("Protect recovery directory/file",result);
}
void protectDirectory(const fs::path& path) { protectObject(path,true); }
fs::path recoveryRoot() {
    const auto root = known(FOLDERID_ProgramData) / L"NetLink";
    protectDirectory(root); protectDirectory(root / L"Recovery"); return root / L"Recovery";
}
std::vector<std::wstring> children(HKEY key) {
    std::vector<std::wstring> result;
    for(DWORD index = 0;; ++index) {
        wchar_t name[256]; DWORD length = 256;
        const auto status = RegEnumKeyExW(key, index, name, &length, nullptr, nullptr, nullptr, nullptr);
        if(status == ERROR_NO_MORE_ITEMS) break;
        if(status != ERROR_SUCCESS) throw Error("Enumerate registry", status);
        result.emplace_back(name, length);
    }
    return result;
}
bool exists(HKEY base, const std::wstring& path, REGSAM view) {
    HKEY key = nullptr;
    const auto result = RegOpenKeyExW(base, path.c_str(), 0, KEY_READ | view, &key);
    if(result == ERROR_FILE_NOT_FOUND || result == ERROR_PATH_NOT_FOUND) return false;
    if(result != ERROR_SUCCESS) throw Error("Open registry", result);
    RegCloseKey(key); return true;
}
std::wstring readString(HKEY base, const std::wstring& path, const wchar_t* name, REGSAM view, DWORD* originalType=nullptr) {
    HKEY key = nullptr;
    const auto opened = RegOpenKeyExW(base, path.c_str(), 0, KEY_READ | view, &key);
    if(opened == ERROR_FILE_NOT_FOUND || opened == ERROR_PATH_NOT_FOUND) return {};
    if(opened != ERROR_SUCCESS) throw Error("Read registry key", opened);
    DWORD type = 0, size = 0;
    auto result = RegQueryValueExW(key, name, nullptr, &type, nullptr, &size);
    std::vector<wchar_t> value(size / sizeof(wchar_t) + 1, 0);
    if(result == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ) && size < 65536)
        result = RegQueryValueExW(key, name, nullptr, &type, reinterpret_cast<BYTE*>(value.data()), &size);
    else if(result == ERROR_SUCCESS) result = ERROR_INVALID_DATA;
    RegCloseKey(key);
    if(result == ERROR_FILE_NOT_FOUND) return {};
    if(result != ERROR_SUCCESS) throw Error("Read registry string", result);
    if(originalType) *originalType=type;
    return value.data();
}
void deleteKey(HKEY base, const std::wstring& path, REGSAM view) {
    HKEY key = nullptr;
    auto result = RegOpenKeyExW(base, path.c_str(), 0, KEY_READ | KEY_WRITE | DELETE | view, &key);
    if(result == ERROR_FILE_NOT_FOUND || result == ERROR_PATH_NOT_FOUND) return;
    if(result != ERROR_SUCCESS) throw Error("Open cleanup key", result);
    result = RegDeleteTreeW(key, nullptr);
    RegCloseKey(key);
    if(result != ERROR_SUCCESS) throw Error("Clean registry tree", result);
    result = RegDeleteKeyExW(base, path.c_str(), view, 0);
    if(result != ERROR_SUCCESS) throw Error("Remove registry key", result);
}
void clearSettings(HKEY base, const std::wstring& path, REGSAM view) {
    // Keys are files, not registry values. Retain custom key locations only.
    const auto auth = path + L"\\Authentication";
    DWORD privateType=REG_SZ,publicType=REG_SZ;
    const auto privatePath = readString(base, auth, L"PrivateKeyBaseDir", view,&privateType);
    const auto publicPath = readString(base, auth, L"PublicKeyBaseDir", view,&publicType);
    deleteKey(base, path, view);
    if(privatePath.empty() && publicPath.empty()) return;
    HKEY key = nullptr;
    const auto status = RegCreateKeyExW(base, auth.c_str(), 0, nullptr, 0, KEY_WRITE | view, nullptr, &key, nullptr);
    if(status != ERROR_SUCCESS) throw Error("Retain key locations", status);
    for(const auto& item : {std::make_pair(L"PrivateKeyBaseDir", privatePath), std::make_pair(L"PublicKeyBaseDir", publicPath)}) {
        if(!item.second.empty()) {
            const auto result = RegSetValueExW(key, item.first, 0, item.first==std::wstring(L"PrivateKeyBaseDir") ? privateType : publicType,
                reinterpret_cast<const BYTE*>(item.second.c_str()), DWORD((item.second.size()+1)*sizeof(wchar_t)));
            if(result != ERROR_SUCCESS) { RegCloseKey(key); throw Error("Write key location", result); }
        }
    }
    RegCloseKey(key);
}
std::wstring expanded(std::wstring value) {
    const auto global = (known(FOLDERID_ProgramData) / L"Veyon").wstring();
    for(const auto& token : {std::wstring(L"%GLOBALAPPDATA%"), std::wstring(L"$GLOBALAPPDATA")}) {
        size_t pos;
        while((pos=value.find(token)) != std::wstring::npos) value.replace(pos, token.size(), global);
    }
    wchar_t output[32768];
    const auto size = ExpandEnvironmentStringsW(value.c_str(), output, 32768);
    require(size && size <= 32768, "Expand key location"); return output;
}
void checkKeysOutside(const fs::path& root, HKEY base = HKEY_LOCAL_MACHINE, const std::wstring& config = configKey) {
    for(auto view : {KEY_WOW64_64KEY, KEY_WOW64_32KEY}) {
        for(const auto name : {L"PrivateKeyBaseDir", L"PublicKeyBaseDir"}) {
            const auto path = readString(base, config+L"\\Authentication", name, view);
            if(!path.empty()) {
                const auto resolved=expanded(path);
                require(fs::path(resolved).is_absolute() && resolved.find(L'%')==std::wstring::npos && resolved.find(L'$')==std::wstring::npos,
                    "Unresolved authentication key location; check it in Configurator before uninstalling");
                noLinks(resolved);
                require(!within(resolved, root), "Authentication keys are inside the application folder; move them with Configurator before uninstalling");
            }
        }
    }
    // Unregistered/manual keys must not disappear in a full directory removal.
    require(!fs::exists(root / L"keys"), "Local keys folder must be relocated before uninstalling");
}
DWORD run(const fs::path& executable, const std::wstring& args, DWORD timeout = 30000) {
    std::wstring command = L"\"" + executable.wstring() + L"\" " + args;
    STARTUPINFOW startup{}; startup.cb=sizeof(startup); PROCESS_INFORMATION process{};
    require(CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
        nullptr, nullptr, &startup, &process), "Start Windows utility");
    CloseHandle(process.hThread);
    const auto waited = WaitForSingleObject(process.hProcess, timeout);
    if(waited != WAIT_OBJECT_0) {
        TerminateProcess(process.hProcess, ERROR_TIMEOUT); WaitForSingleObject(process.hProcess, 2000);
        CloseHandle(process.hProcess); throw Error("Windows utility deadline exceeded", ERROR_TIMEOUT);
    }
    DWORD status = ERROR_GEN_FAILURE; GetExitCodeProcess(process.hProcess, &status); CloseHandle(process.hProcess); return status;
}
void copyBackups(const fs::path& source, const fs::path& destination) {
    if(!fs::exists(source)) return;
    noLinks(source); protectDirectory(destination);
    for(const auto& entry : fs::directory_iterator(source)) {
        noLinks(entry.path());
        if(entry.is_directory()) copyBackups(entry.path(), destination / entry.path().filename());
        else { const auto file=destination/entry.path().filename(); fs::copy_file(entry.path(),file); protectObject(file,false); }
    }
}
void backup(const fs::path& root) {
    SYSTEMTIME now{}; GetSystemTime(&now); GUID id{}; require(SUCCEEDED(CoCreateGuid(&id)), "Recovery identifier");
    wchar_t guid[40]; require(StringFromGUID2(id,guid,40)>0,"Recovery identifier format");
    wchar_t suffix[128]; swprintf(suffix, 128, L"%04u%02u%02u-%02u%02u%02u-%ls", now.wYear, now.wMonth,
        now.wDay, now.wHour, now.wMinute, now.wSecond, guid);
    const auto destination = recoveryRoot() / suffix; protectDirectory(destination);
    for(auto view : {KEY_WOW64_64KEY, KEY_WOW64_32KEY}) {
        if(!exists(HKEY_LOCAL_MACHINE, configKey, view)) continue;
        const auto file = destination / (view == KEY_WOW64_64KEY ? L"configuration-64.reg" : L"configuration-32.reg");
        require(run(known(FOLDERID_System) / L"reg.exe", L"export \"HKLM\\" + std::wstring(configKey) +
            L"\" \"" + file.wstring() + L"\" /y /reg:" + (view == KEY_WOW64_64KEY ? L"64" : L"32")) == 0, "Backup registry configuration");
    }
    copyBackups(root / L"Backups", destination / L"previous-backups");
    std::wofstream latest(recoveryRoot() / L"latest.txt"); latest << destination.wstring();
    require(bool(latest), "Write recovery location");
}
void stopService(const fs::path& root) {
    SC_HANDLE manager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    require(manager != nullptr, "Open service manager");
    SC_HANDLE service = OpenServiceW(manager, L"VeyonSchoolService", SERVICE_QUERY_CONFIG | SERVICE_QUERY_STATUS | SERVICE_STOP | DELETE);
    if(!service) { auto error=GetLastError(); CloseServiceHandle(manager); if(error==ERROR_SERVICE_DOES_NOT_EXIST) return; throw Error("Open NetLink service", error); }
    try {
        DWORD size=0; QueryServiceConfigW(service, nullptr, 0, &size); std::vector<BYTE> data(size);
        require(QueryServiceConfigW(service, reinterpret_cast<QUERY_SERVICE_CONFIGW*>(data.data()), size, &size), "Read service owner");
        auto binary = std::wstring(reinterpret_cast<QUERY_SERVICE_CONFIGW*>(data.data())->lpBinaryPathName);
        // Exact service executable, no arguments or another application's path.
        bool owned = false;
        for(const auto name : {L"netlink-service.exe", L"veyon-service.exe"}) {
            const auto path=(root/name).wstring(); owned |= equal(binary, L"\""+path+L"\"") || equal(binary,path);
        }
        require(owned, "Service belongs to another location");
        SERVICE_STATUS status{}; require(QueryServiceStatus(service,&status), "Read service status");
        if(status.dwCurrentState != SERVICE_STOPPED && status.dwCurrentState != SERVICE_STOP_PENDING)
            require(ControlService(service, SERVICE_CONTROL_STOP, &status), "Stop NetLink service");
        const auto deadline=GetTickCount64()+30000;
        do {
            require(QueryServiceStatus(service,&status), "Wait for service");
            if(status.dwCurrentState==SERVICE_STOPPED) break;
            Sleep(100);
        } while(GetTickCount64()<deadline);
        require(status.dwCurrentState==SERVICE_STOPPED, "Service stop timed out");
        require(DeleteService(service), "Unregister NetLink service");
    } catch(...) { CloseServiceHandle(service); CloseServiceHandle(manager); throw; }
    CloseServiceHandle(service);
    const auto deadline=GetTickCount64()+10000;
    do {
        service=OpenServiceW(manager,L"VeyonSchoolService",SERVICE_QUERY_STATUS);
        if(!service && GetLastError()==ERROR_SERVICE_DOES_NOT_EXIST) { CloseServiceHandle(manager); return; }
        if(service) CloseServiceHandle(service);
        Sleep(100);
    } while(GetTickCount64()<deadline);
    CloseServiceHandle(manager); throw Error("Service deletion is pending; close Services and reboot", ERROR_SERVICE_MARKED_FOR_DELETE);
}
bool originalPresent() {
    return exists(HKEY_LOCAL_MACHINE,L"SYSTEM\\CurrentControlSet\\Services\\VeyonService",KEY_WOW64_64KEY) ||
        exists(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Veyon",KEY_WOW64_64KEY) ||
        exists(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Veyon",KEY_WOW64_32KEY) ||
        fs::exists(known(FOLDERID_ProgramFilesX64)/L"Veyon"/L"veyon-service.exe");
}
void cleanupRegistry() {
    const bool shared = originalPresent();
    for(auto view : {KEY_WOW64_64KEY,KEY_WOW64_32KEY}) {
        if(!shared) clearSettings(HKEY_LOCAL_MACHINE,configKey,view);
    }
    for(const auto& sid : children(HKEY_USERS)) {
        PSID parsed=nullptr;
        if(!ConvertStringSidToSidW(sid.c_str(),&parsed)) continue;
        LocalFree(parsed);
        for(auto view : {KEY_WOW64_64KEY,KEY_WOW64_32KEY}) {
            deleteKey(HKEY_USERS,sid+L"\\"+appearanceKey,view);
            if(!shared) clearSettings(HKEY_USERS,sid+L"\\"+configKey,view);
        }
    }
    const auto events=std::wstring(L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application");
    HKEY eventRoot=nullptr;
    if(RegOpenKeyExW(HKEY_LOCAL_MACHINE,events.c_str(),0,KEY_READ,&eventRoot)==ERROR_SUCCESS) {
        std::vector<std::wstring> names;
        try { names=children(eventRoot); } catch(...) { RegCloseKey(eventRoot); throw; }
        RegCloseKey(eventRoot);
        const auto root=installRoot();
        for(const auto& name:names) {
            // Upstream also uses bare names such as Service and Master-1.
            // Ownership is the exact message-file path, never a generic name search.
            bool component=name.rfind(L"Veyon",0)==0 || name.rfind(L"NetLink",0)==0;
            for(const auto base:{L"CLI",L"Configurator",L"Master",L"Server",L"Service",L"Worker",L"Companion"}) {
                const auto prefix=std::wstring(base)+L"-";
                component |= equal(name,base) || (name.rfind(prefix,0)==0 && name.size()>prefix.size() &&
                    std::all_of(name.begin()+prefix.size(),name.end(),[](wchar_t c){return c>=L'0' && c<=L'9';}));
            }
            if(!component) continue;
            if(shared && name.rfind(L"NetLink",0)!=0) continue;
            std::wstring path;
            try { path=readString(HKEY_LOCAL_MACHINE,events+L"\\"+name,L"EventMessageFile",KEY_WOW64_64KEY); }
            catch(const Error& error) { if(error.code==ERROR_INVALID_DATA) continue; throw; }
            if(!path.empty() && within(expanded(path),root)) deleteKey(HKEY_LOCAL_MACHINE,events+L"\\"+name,KEY_WOW64_64KEY);
        }
    }
}
bool removeTree(const fs::path& path, bool allowReboot) {
    auto attributes=GetFileAttributesW(path.c_str());
    if(attributes==INVALID_FILE_ATTRIBUTES) {
        const auto error=GetLastError();
        if(error==ERROR_FILE_NOT_FOUND || error==ERROR_PATH_NOT_FOUND) return false;
        throw Error("Inspect removal target",error);
    }
    bool reboot=false;
    // Never follow directory junctions or symbolic links to another target.
    if((attributes & FILE_ATTRIBUTE_DIRECTORY) && !(attributes & FILE_ATTRIBUTE_REPARSE_POINT))
        for(const auto& entry : fs::directory_iterator(path)) reboot |= removeTree(entry.path(),allowReboot);
    if((attributes & FILE_ATTRIBUTE_READONLY) && !(attributes & FILE_ATTRIBUTE_REPARSE_POINT))
        require(SetFileAttributesW(path.c_str(),attributes & ~FILE_ATTRIBUTE_READONLY),"Clear read-only application file");
    const bool removed = (attributes & FILE_ATTRIBUTE_DIRECTORY) ? RemoveDirectoryW(path.c_str()) : DeleteFileW(path.c_str());
    if(!removed) {
        const auto error=GetLastError();
        if(!allowReboot || (error!=ERROR_SHARING_VIOLATION && error!=ERROR_ACCESS_DENIED && error!=ERROR_DIR_NOT_EMPTY))
            throw Error("Remove application file",error);
        require(MoveFileExW(path.c_str(),nullptr,MOVEFILE_DELAY_UNTIL_REBOOT),"Schedule removal at reboot"); reboot=true;
    }
    return reboot;
}
bool pendingRemoval(const fs::path& root) {
    HKEY key=nullptr;
    const auto opened=RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"SYSTEM\\CurrentControlSet\\Control\\Session Manager",0,KEY_READ,&key);
    if(opened!=ERROR_SUCCESS) throw Error("Read pending reboot operations",opened);
    DWORD size=0,type=0; auto result=RegQueryValueExW(key,L"PendingFileRenameOperations",nullptr,&type,nullptr,&size);
    if(result==ERROR_FILE_NOT_FOUND) { RegCloseKey(key); return false; }
    std::vector<wchar_t> data(size/2+2,0);
    if(result==ERROR_SUCCESS && type==REG_MULTI_SZ) result=RegQueryValueExW(key,L"PendingFileRenameOperations",nullptr,&type,reinterpret_cast<BYTE*>(data.data()),&size);
    else if(result==ERROR_SUCCESS) result=ERROR_INVALID_DATA;
    RegCloseKey(key); if(result!=ERROR_SUCCESS) throw Error("Read reboot operations",result);
    for(size_t pos=0; pos+1<data.size() && data[pos];) {
        const std::wstring source(data.data()+pos); pos+=source.size()+1;
        if(pos>=data.size()) throw Error("Invalid reboot data",ERROR_INVALID_DATA);
        const std::wstring target(data.data()+pos); pos+=target.size()+1;
        auto path=source; if(path.rfind(L"\\??\\",0)==0) path.erase(0,4);
        if(within(path,root)) return true;
    }
    return false;
}
void verify(const fs::path& root) {
    noLinks(root);
    HCRYPTPROV provider=0; require(CryptAcquireContextW(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT),"SHA256 provider");
    try {
        std::ifstream manifest(root/L"payload.sha256"); require(bool(manifest),"Payload manifest missing");
        std::string line; unsigned count=0;
        while(std::getline(manifest,line)) {
            if(!line.empty() && line.back()=='\r') line.pop_back();
            require(line.size()>66 && line.substr(64,2)=="  ","Invalid payload manifest");
            const auto relative=fs::u8path(line.substr(66));
            require(!relative.is_absolute() && within(root/relative,root) && relative.native().find(L':')==std::wstring::npos,"Unsafe payload path");
            noLinks(root/relative); std::ifstream input(root/relative,std::ios::binary); require(bool(input),"Payload file missing");
            HCRYPTHASH hash=0; require(CryptCreateHash(provider,CALG_SHA_256,0,0,&hash),"SHA256 initialization");
            char buffer[65536];
            bool ok=true;
            while(input.read(buffer,sizeof(buffer)) || input.gcount())
                if(!CryptHashData(hash,reinterpret_cast<const BYTE*>(buffer),DWORD(input.gcount()),0)) { ok=false; break; }
            BYTE digest[32]; DWORD length=32;
            ok=ok && input.eof() && CryptGetHashParam(hash,HP_HASHVAL,digest,&length,0);
            CryptDestroyHash(hash); require(ok,"Read/hash payload");
            std::string actual; const char* hex="0123456789abcdef";
            for(auto byte:digest) { actual+=hex[byte>>4]; actual+=hex[byte&15]; }
            auto expected=line.substr(0,64); std::transform(expected.begin(),expected.end(),expected.begin(),[](unsigned char c){return char(std::tolower(c));});
            require(actual==expected,"Payload hash mismatch"); ++count;
        }
        require(manifest.eof() && count>0,"Incomplete payload manifest");
    } catch(...) { CryptReleaseContext(provider,0); throw; }
    CryptReleaseContext(provider,0);
}
void cleanupFirewall(const fs::path& root) {
    const auto initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    require(SUCCEEDED(initialized),"Initialize firewall management");
    INetFwPolicy2* policy=nullptr; INetFwRules* rules=nullptr; IUnknown* unknown=nullptr; IEnumVARIANT* enumerator=nullptr;
    HRESULT result=CoCreateInstance(__uuidof(NetFwPolicy2),nullptr,CLSCTX_INPROC_SERVER,__uuidof(INetFwPolicy2),reinterpret_cast<void**>(&policy));
    if(SUCCEEDED(result)) result=policy->get_Rules(&rules);
    if(SUCCEEDED(result)) result=rules->get__NewEnum(&unknown);
    if(SUCCEEDED(result)) result=unknown->QueryInterface(IID_IEnumVARIANT,reinterpret_cast<void**>(&enumerator));
    std::vector<std::wstring> names;
    if(SUCCEEDED(result)) {
        VARIANT value; VariantInit(&value);
        while(enumerator->Next(1,&value,nullptr)==S_OK) {
            INetFwRule* rule=nullptr;
            if(value.vt==VT_DISPATCH && SUCCEEDED(value.pdispVal->QueryInterface(__uuidof(INetFwRule),reinterpret_cast<void**>(&rule)))) {
                BSTR app=nullptr,name=nullptr; rule->get_ApplicationName(&app); rule->get_Name(&name);
                if(app && *app && name && within(app,root)) names.emplace_back(name);
                SysFreeString(app); SysFreeString(name); rule->Release();
            }
            VariantClear(&value);
        }
        for(const auto& name:names) { BSTR text=SysAllocString(name.c_str()); result=rules->Remove(text); SysFreeString(text); if(FAILED(result)) break; }
    }
    if(enumerator) enumerator->Release(); if(unknown) unknown->Release(); if(rules) rules->Release(); if(policy) policy->Release();
    CoUninitialize(); require(SUCCEEDED(result),"Remove fork firewall rules");
}
} // namespace Maintenance

#ifndef NETLINK_MAINTENANCE_LIBRARY
int wmain(int argc,wchar_t** argv) {
    using namespace Maintenance;
    try {
        if(argc!=2) return ERROR_INVALID_PARAMETER;
        const std::wstring action=argv[1]; const auto root=installRoot();
        if(action==L"verify") { verify(root); return 0; }
        if(action==L"check-pending") return pendingRemoval(root) ? ERROR_SUCCESS_REBOOT_REQUIRED : 0;
        if(action==L"check-keys") { checkKeysOutside(root); return 0; }
        if(action==L"backup") backup(root);
        else if(action==L"stop") stopService(root);
        else if(action==L"clean-settings") cleanupRegistry();
        else if(action==L"clean-firewall") cleanupFirewall(root);
        else if(action==L"clean-files") { checkKeysOutside(root); return removeTree(root,true) ? ERROR_SUCCESS_REBOOT_REQUIRED : 0; }
        else return ERROR_INVALID_PARAMETER;
        return 0;
    } catch(const Error& error) {
        std::cerr << error.what() << " (Windows " << error.code << ")\n";
        try { std::ofstream log(recoveryRoot().parent_path()/L"maintenance-error.txt"); log << error.what() << " (Windows " << error.code << ")\n"; } catch(...) {}
        return error.code ? int(error.code) : ERROR_GEN_FAILURE;
    } catch(const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { std::ofstream log(recoveryRoot().parent_path()/L"maintenance-error.txt"); log << error.what() << '\n'; } catch(...) {}
        return ERROR_GEN_FAILURE;
    }
}
#endif
