// SPDX-License-Identifier: GPL-2.0-or-later
// No service/configuration operations. Refuse to run before the full payload.
#include <windows.h>
#include <shellapi.h>
#include <filesystem>
#include <fstream>
static int runCli(int argc,wchar_t** argv) {
    namespace fs=std::filesystem;
    wchar_t file[32768]; if(!GetModuleFileNameW(nullptr,file,32768)) return 2;
    const auto root=fs::path(file).parent_path();
    if(!fs::exists(root/L"platforms"/L"qwindows.dll") || !fs::exists(root/L"payload.sha256")) return 3;
    if(fs::exists(root.parent_path()/L"cli-failure-fixture.txt")) return 9;
    std::wofstream log(root.parent_path()/L"cli-actions.txt",std::ios::app);
    for(int i=1;i<argc;++i) log<<argv[i]<<L' '; log<<L'\n'; return log ? 0 : 4;
}
// Match the real netlink-wcli subsystem. A console fixture can receive console
// close/Ctrl-C events unrelated to installer behavior and exit 0xC000013A.
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int) {
    int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    if(!argv) return 2;
    const auto result=runCli(argc,argv);LocalFree(argv);return result;
}
