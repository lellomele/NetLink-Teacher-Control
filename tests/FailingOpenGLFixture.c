/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Test-only DLL: simulate ERROR_DLL_INIT_FAILED without changing any driver. */
#include <windows.h>
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID reserved) {
    (void)module;
    (void)reserved;
    return reason != DLL_PROCESS_ATTACH;
}
