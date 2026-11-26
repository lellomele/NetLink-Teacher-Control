// SPDX-License-Identifier: GPL-2.0-or-later
// Pure path-classification test; this test driver never opens a process or SCM.
#define wWinMain UnusedProductionEntry
#include "../packaging/UpgradePrepare.cpp"
#undef wWinMain
#include <iostream>
int main(){
    const fs::path root=L"C:\\NetLinkFixture";
    const bool good=ownedProcess(root/L"netlink-worker.exe",root) &&
        ownedProcess(root/L"NETLINK-MASTER.EXE",root) &&
        !ownedProcess(L"C:\\Other\\netlink-worker.exe",root) &&
        !ownedProcess(root/L"subdir"/L"netlink-worker.exe",root) &&
        !ownedProcess(root/L"personal.exe",root) &&
        !ownedProcess(root/L"netlink-worker.exe.bak",root);
    std::cout<<(good?"PASS":"FAIL")<<": upgrade preparation process scope (six synthetic names)\n";
    return good?0:1;
}
