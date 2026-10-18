// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#define NETLINK_MAINTENANCE_LIBRARY
#include "../packaging/Maintenance.cpp"

using namespace Maintenance;
unsigned passed=0;
void check(bool ok,const char* name) { if(!ok) throw std::runtime_error(name); ++passed; }
template<class F> void rejected(F function,const char* name) {
    bool failed=false; try { function(); } catch(const std::exception&) { failed=true; }
    check(failed,name);
}
void write(const fs::path& path,const std::string& text) { fs::create_directories(path.parent_path()); std::ofstream file(path,std::ios::binary); file<<text; check(bool(file),"Write fixture"); }
int wmain(int argc,wchar_t** argv) {
    if(argc!=2) return 2;
    const fs::path root=full(argv[1]);
    const auto registry=L"Software\\NetLinkMaintenanceTests\\"+root.filename().wstring();
    try {
        check(root.filename().wstring().rfind(L"maintenance-",0)==0,"Private fixture root required");
        noLinks(root); fs::create_directories(root);
        check(within(root/L"child",root),"Containment");
        check(!within(root/L".."/L"outside",root),"Traversal denied");
        check(!within(root.wstring()+L"-sibling",root),"Sibling prefix denied");
        write(root/L"runtime"/L"data.bin","abc");
        write(root/L"runtime"/L"payload.sha256","ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad  data.bin\n");
        verify(root/L"runtime"); ++passed;
        write(root/L"runtime"/L"data.bin","bad");
        rejected([&]{verify(root/L"runtime");},"Corrupt payload refused");
        fs::remove(root/L"runtime"/L"data.bin");
        rejected([&]{verify(root/L"runtime");},"Missing payload refused");
        write(root/L"runtime"/L"payload.sha256",std::string(64,'0')+"  ../outside.bin\n");
        rejected([&]{verify(root/L"runtime");},"Manifest cannot escape root");
        write(root/L"runtime"/L"payload.sha256","");
        rejected([&]{verify(root/L"runtime");},"Empty manifest refused");
        const auto replacements=root/L"replacement.sha256";
        const auto application=root/L"preflight";
        write(replacements,std::string(64,'0')+"  vnchooks.dll\n");
        checkReplacementFiles(application,replacements); ++passed;
        write(application/L"vnchooks.dll","PRESERVE CONTENTS");
        const auto attributesBefore=GetFileAttributesW((application/L"vnchooks.dll").c_str());
        checkReplacementFiles(application,replacements); ++passed;
        check(fs::file_size(application/L"vnchooks.dll")==17,"Preflight never truncates existing files");
        check(GetFileAttributesW((application/L"vnchooks.dll").c_str())==attributesBefore,"Preflight preserves attributes");
        auto heldReplacement=CreateFileW((application/L"vnchooks.dll").c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
        check(heldReplacement!=INVALID_HANDLE_VALUE,"Hold replacement open");
        DWORD busyCode=0;
        try {checkReplacementFiles(application,replacements);} catch(const Error& error) {busyCode=error.code;}
        CloseHandle(heldReplacement);
        check(busyCode==ERROR_SUCCESS_REBOOT_REQUIRED,"Locked replacement explicitly requires reboot");
        checkReplacementFiles(application,replacements); ++passed;
        fs::remove(application/L"vnchooks.dll");
        fs::copy_file(known(FOLDERID_System)/L"version.dll",application/L"vnchooks.dll");
        auto image=LoadLibraryW((application/L"vnchooks.dll").c_str());
        check(image!=nullptr,"Load real DLL image fixture");
        busyCode=0;
        try {checkReplacementFiles(application,replacements);} catch(const Error& error) {busyCode=error.code;}
        FreeLibrary(image);
        check(busyCode==ERROR_SUCCESS_REBOOT_REQUIRED,"Mapped DLL image explicitly requires reboot");
        checkReplacementFiles(application,replacements); ++passed;
        for(const auto& invalid: {std::string(64,'0')+"  ../outside\n", std::string(64,'0')+"  C:/outside\n",
                std::string(64,'0')+"  data.bin:stream\n",std::string(64,'0')+"  folder/../data.bin\n",std::string{}}) {
            write(replacements,invalid);rejected([&]{checkReplacementFiles(application,replacements);},"Invalid replacement plan rejected");
        }
        write(replacements,std::string(64,'0')+"  data.bin\n");
        write(application/L"data.bin","read only");
        check(SetFileAttributesW((application/L"data.bin").c_str(),FILE_ATTRIBUTE_READONLY),"Read-only replacement fixture");
        DWORD deniedCode=0;
        try {checkReplacementFiles(application,replacements);} catch(const Error& error) {deniedCode=error.code;}
        check(deniedCode==ERROR_ACCESS_DENIED,"Read-only is not falsely reported as a reboot lock");
        check(SetFileAttributesW((application/L"data.bin").c_str(),FILE_ATTRIBUTE_NORMAL),"Restore fixture attributes");
        HKEY key=nullptr;
        check(RegCreateKeyExW(HKEY_CURRENT_USER,(registry+L"\\Authentication").c_str(),0,nullptr,0,KEY_ALL_ACCESS,nullptr,&key,nullptr)==ERROR_SUCCESS,"Fixture registry");
        const auto privatePath=(root/L"preserved-keys"/L"private").wstring();
        const auto publicPath=(root/L"preserved-keys"/L"public").wstring();
        for(const auto& item:{std::make_pair(L"PrivateKeyBaseDir",privatePath),std::make_pair(L"PublicKeyBaseDir",publicPath),std::make_pair(L"OtherSetting",std::wstring(L"remove"))})
            check(RegSetValueExW(key,item.first,0,REG_SZ,reinterpret_cast<const BYTE*>(item.second.c_str()),DWORD((item.second.size()+1)*2))==ERROR_SUCCESS,"Write fixture setting");
        RegCloseKey(key);
        check(RegOpenKeyExW(HKEY_CURRENT_USER,(registry+L"\\Authentication").c_str(),0,KEY_WRITE,&key)==ERROR_SUCCESS,"Open key-type fixture");
        check(RegSetValueExW(key,L"PublicKeyBaseDir",0,REG_EXPAND_SZ,reinterpret_cast<const BYTE*>(publicPath.c_str()),DWORD((publicPath.size()+1)*2))==ERROR_SUCCESS,"Expandable path fixture");
        RegCloseKey(key);
        checkKeysOutside(root/L"application",HKEY_CURRENT_USER,registry); ++passed;
        rejected([&]{checkKeysOutside(root,HKEY_CURRENT_USER,registry);},"Custom keys inside root stop removal");
        clearSettings(HKEY_CURRENT_USER,registry,KEY_WOW64_64KEY);
        check(readString(HKEY_CURRENT_USER,registry+L"\\Authentication",L"PrivateKeyBaseDir",KEY_WOW64_64KEY)==privatePath,"Retain private key location");
        check(readString(HKEY_CURRENT_USER,registry+L"\\Authentication",L"PublicKeyBaseDir",KEY_WOW64_64KEY)==publicPath,"Retain public key location");
        DWORD retainedType=0;
        readString(HKEY_CURRENT_USER,registry+L"\\Authentication",L"PublicKeyBaseDir",KEY_WOW64_64KEY,&retainedType);
        check(retainedType==REG_EXPAND_SZ,"Preserve registry key-location type");
        check(readString(HKEY_CURRENT_USER,registry+L"\\Authentication",L"OtherSetting",KEY_WOW64_64KEY).empty(),"Remove other settings");
        clearSettings(HKEY_CURRENT_USER,registry,KEY_WOW64_64KEY); ++passed;
        write(root/L"application"/L"nested"/L"unknown.txt","remove");
        write(root/L"preserved-keys"/L"private"/L"netlink_test","INERT PRIVATE KEY FIXTURE");
        write(root/L"preserved-keys"/L"public"/L"netlink_test","INERT PUBLIC KEY FIXTURE");
        check(SetFileAttributesW((root/L"application"/L"nested"/L"unknown.txt").c_str(),FILE_ATTRIBUTE_READONLY),"Set read-only fixture");
        check(!removeTree(root/L"application",false),"Uninstall whole application tree");
        check(!fs::exists(root/L"application"),"Application folder removed");
        check(fs::file_size(root/L"preserved-keys"/L"private"/L"netlink_test")==25,"Private key fixture retained");
        check(fs::file_size(root/L"preserved-keys"/L"public"/L"netlink_test")==24,"Public key fixture retained");
        write(root/L"locked-case"/L"in-use.dll","LOCKED FIXTURE");
        const auto held=CreateFileW((root/L"locked-case"/L"in-use.dll").c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
        check(held!=INVALID_HANDLE_VALUE,"Hold fixture DLL open");
        rejected([&]{removeTree(root/L"locked-case",false);},"Locked file is reported, not silently ignored");
        CloseHandle(held);
        check(!removeTree(root/L"locked-case",false),"Locked file removed after handle closes");
        // The PowerShell harness supplies a junction to an outside marker.
        if(fs::exists(root/L"junction-case")) {
            rejected([&]{noLinks(root/L"junction-case"/L"redirect");},"Redirected root refused");
            check(!removeTree(root/L"junction-case",false),"Remove junction object, never target");
            check(fs::exists(root/L"outside"/L"must-survive.txt"),"Junction target retained");
        }
        fs::create_directories(root/L"application");
        write(root/L"application"/L"netlink-wcli.exe","INERT LEFTOVER EXE - MUST NOT EXECUTE");
        rejected([&]{verify(root/L"application");},"Interrupted install cannot pass verification");
        write(root/L"application"/L"data.bin","abc");
        write(root/L"application"/L"payload.sha256","ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad  data.bin\n");
        verify(root/L"application"); ++passed;
        check(!removeTree(root/L"application",false),"Reinstallation removed cleanly");
        deleteKey(HKEY_CURRENT_USER,registry,KEY_WOW64_64KEY);
        check(!exists(HKEY_CURRENT_USER,registry,KEY_WOW64_64KEY),"Fixture registry removed");
        std::cout<<"PASS: "<<passed<<" maintenance assertions; isolated files/HKCU only, no services/firewall/reboot state changed.\n";
        return 0;
    } catch(const Error& error) { std::cerr<<error.what()<<" Windows="<<error.code<<'\n'; }
      catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; }
    try { deleteKey(HKEY_CURRENT_USER,registry,KEY_WOW64_64KEY); } catch(...) {}
    return 1;
}
