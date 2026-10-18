// SPDX-License-Identifier: GPL-2.0-or-later
#define NETLINK_MAINTENANCE_LIBRARY
#include "../packaging/Maintenance.cpp"
int wmain(int argc,wchar_t** argv) {
    using namespace Maintenance;
    fs::path diagnostic;
    try {
        if(argc!=3 && argc!=4) return 2;
        const auto root=fs::path(full(argv[2])); const auto parent=root.parent_path();
        require(root.filename()==L"NetLinkFixtureApp" && parent.filename().wstring().rfind(L"installer-roundtrip-",0)==0 &&
            fs::exists(parent/L".netlink-install-fixture"),"Not an isolated installer fixture");
        noLinks(parent);
        diagnostic=parent/L"fixture-error.txt";
        const auto config=L"Software\\NetLinkInstallerTests\\"+parent.filename().wstring()+L"\\Configuration";
        const std::wstring action=argv[1];
        { std::wofstream log(parent/L"maintenance-actions.txt",std::ios::app); log<<action<<L'\n'; }
        if(action==L"hold-image") {
            auto image=LoadLibraryW((root/L"vnchooks.dll").c_str());require(image!=nullptr,"Fixture DLL load");
            {std::ofstream ready(parent/L"image-ready.txt");ready<<"ready";}
            const auto deadline=GetTickCount64()+90000;
            while(!fs::exists(parent/L"image-release.txt") && GetTickCount64()<deadline) Sleep(20);
            FreeLibrary(image);return 0;
        }
        if(action==L"check-files") { require(argc==4,"Fixture replacement manifest required"); checkReplacementFiles(root,argv[3]); }
        else if(action==L"verify") verify(root);
        else if(action==L"backup") {
            const auto backupKey=config+L"-backup";
            HKEY source=nullptr,target=nullptr;
            if(RegOpenKeyExW(HKEY_CURRENT_USER,config.c_str(),0,KEY_READ,&source)==ERROR_SUCCESS) {
                require(RegCreateKeyExW(HKEY_CURRENT_USER,backupKey.c_str(),0,nullptr,0,KEY_ALL_ACCESS,nullptr,&target,nullptr)==ERROR_SUCCESS,"Fixture backup");
                const auto status=RegCopyTreeW(source,nullptr,target); RegCloseKey(source); RegCloseKey(target); require(status==ERROR_SUCCESS,"Copy fixture settings");
            }
        } else if(action==L"check-keys") checkKeysOutside(root,HKEY_CURRENT_USER,config);
        else if(action==L"clean-settings") clearSettings(HKEY_CURRENT_USER,config,KEY_WOW64_64KEY);
        else if(action==L"clean-files") { checkKeysOutside(root,HKEY_CURRENT_USER,config); removeTree(root,false); }
        else if(action!=L"stop" && action!=L"quiesce" && action!=L"check-pending" && action!=L"clean-firewall") return 2;
        return 0;
    } catch(const Error& error) { std::cerr<<error.what()<<" Windows="<<error.code<<'\n'; if(!diagnostic.empty()) { std::ofstream log(diagnostic); log<<error.what()<<" Windows="<<error.code; } return error.code?int(error.code):1; }
      catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; if(!diagnostic.empty()) { std::ofstream log(diagnostic); log<<error.what(); } }
    return 1;
}
