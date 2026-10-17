// SPDX-License-Identifier: GPL-2.0-or-later
// No service/configuration operations. Refuse to run before the full payload.
#include <windows.h>
#include <filesystem>
#include <fstream>
int wmain(int argc,wchar_t** argv) {
    namespace fs=std::filesystem;
    wchar_t file[32768]; if(!GetModuleFileNameW(nullptr,file,32768)) return 2;
    const auto root=fs::path(file).parent_path();
    if(!fs::exists(root/L"platforms"/L"qwindows.dll") || !fs::exists(root/L"payload.sha256")) return 3;
    std::wofstream log(root.parent_path()/L"cli-actions.txt",std::ios::app);
    for(int i=1;i<argc;++i) log<<argv[i]<<L' '; log<<L'\n'; return log ? 0 : 4;
}
