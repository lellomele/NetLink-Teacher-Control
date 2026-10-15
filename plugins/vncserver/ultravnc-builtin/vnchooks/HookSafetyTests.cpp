// Copyright (c) 2025-26 prof. Ing. Raffaele Mele
// No hook is ever installed by this test. Only the two hook APIs and queue
// delivery are replaced; mapping/atomics/ownership use the real Windows APIs.
#include <windows.h>
#include <stdio.h>

static int installCalls = 0, removeCalls = 0, nextCalls = 0, postCalls = 0;
static int failingInstall = -1, failingRemove = -1;
static BOOL postSucceeds = TRUE;
static BOOL unpublishedDuringInstall = TRUE;
static BOOL recursivePost = FALSE;
static BOOL WINAPI FakeUnhook(HHOOK);
static HHOOK WINAPI FakeInstall(int, HOOKPROC, HINSTANCE, DWORD);
static LRESULT WINAPI FakeNext(HHOOK, int, WPARAM, LPARAM);
static BOOL WINAPI FakePostThread(DWORD, UINT, WPARAM, LPARAM);
static BOOL WINAPI FakePostWindow(HWND, UINT, WPARAM, LPARAM);

#define VNCHOOKS_MAPPING_NAME "Local\\NetLinkVncHooks.IsolatedTests-v2"
#define VNCHOOKS_MUTEX_NAME "Local\\NetLinkVncHooks.IsolatedTests-owner-v2"
#define SetWindowsHookExA FakeInstall
#define UnhookWindowsHookEx FakeUnhook
#define CallNextHookEx FakeNext
#define PostThreadMessageA FakePostThread
#define PostMessageA FakePostWindow
#include "VNCHooks.cpp"
#undef SetWindowsHookExA
#undef UnhookWindowsHookEx
#undef CallNextHookEx
#undef PostThreadMessageA
#undef PostMessageA

static HHOOK WINAPI FakeInstall(int type, HOOKPROC, HINSTANCE, DWORD) {
	++installCalls;
	if (ReadLong(&g_shared->threadId)) unpublishedDuringInstall = FALSE;
	if (type == failingInstall) { SetLastError(ERROR_NOT_ENOUGH_MEMORY); return NULL; }
	return (HHOOK)(INT_PTR)(type + 20);
}
static BOOL WINAPI FakeUnhook(HHOOK hook) {
	++removeCalls;
	if ((INT_PTR)hook == failingRemove + 20) { SetLastError(ERROR_INVALID_HOOK_HANDLE); return FALSE; }
	return TRUE;
}
static LRESULT WINAPI FakeNext(HHOOK, int, WPARAM, LPARAM) { ++nextCalls; return 17; }
static BOOL WINAPI FakePostThread(DWORD, UINT, WPARAM, LPARAM) { ++postCalls; return postSucceeds; }
static BOOL WINAPI FakePostWindow(HWND, UINT message, WPARAM w, LPARAM l) {
	++postCalls;
	if (recursivePost) {
		const CWPSTRUCT nested = {l, w, message, NULL};
		CallWndProc(HC_ACTION, 0, (LPARAM)&nested);
	}
	return postSucceeds;
}

static volatile LONG stressInside = 0, stressOverlap = 0;
static DWORD WINAPI CallbackStress(LPVOID) {
	for (int i = 0; i < 10000; ++i) {
		if (BeginCallback()) {
			if (InterlockedIncrement(&stressInside) != 1) InterlockedIncrement(&stressOverlap);
			InterlockedDecrement(&stressInside);
			InterlockedExchange(&g_callback, 0);
		}
	}
	return 0;
}

#define CHECK(expr) do { if (!(expr)) { printf("FAIL line %d: %s\n", __LINE__, #expr); return __LINE__; } } while (0)

int main(int argc, char **argv) {
	if (argc == 3 && lstrcmpA(argv[1], "--load-only") == 0) {
		DWORD before = 0, after = 0;
		GetProcessHandleCount(GetCurrentProcess(), &before);
		// Exercise the actual CRT-free DLL without ever calling SetHooks.
		for (int i = 0; i < 200; ++i) {
			HMODULE module = LoadLibraryA(argv[2]);
			CHECK(module);
			typedef DWORD (*TypeFunction)();
			TypeFunction type = (TypeFunction)GetProcAddress(module, "HooksType");
			CHECK(type && type() == 0);
			typedef BOOL (*DiagnosticFunction)(VNCHOOKS_DIAGNOSTICS *);
			DiagnosticFunction diagnostic = (DiagnosticFunction)GetProcAddress(module, "GetHookDiagnostics");
			VNCHOOKS_DIAGNOSTICS data = {};
			data.size = sizeof(data);
			CHECK(diagnostic && diagnostic(&data) && data.version == 1);
			CHECK(FreeLibrary(module));
		}
		GetProcessHandleCount(GetCurrentProcess(), &after);
		CHECK(after <= before + 2);
		printf("PASS: 200 isolated LoadLibrary/initialize/FreeLibrary cycles; handles %lu -> %lu; no hooks installed.\n", before, after);
		return 0;
	}
	CHECK(DllMain(GetModuleHandleA(NULL), DLL_PROCESS_ATTACH, NULL));
	CHECK(!g_shared && !g_mapping && !g_initialization); // DllMain is inert
	CHECK(!SetHooks(0, WM_APP, 0, WM_APP + 1, FALSE));
	CHECK(installCalls == 0);
	CHECK(EnsureInitialized());
	const DWORD thread = GetCurrentThreadId();
	CHECK(SetHooks(thread, WM_APP, WM_APP + 1, WM_APP + 2, FALSE));
	CHECK(installCalls == 3 && unpublishedDuringInstall);
	CHECK((DWORD)ReadLong(&g_shared->threadId) == thread);
	CHECK(!SetHooks(thread, WM_APP, 0, WM_APP + 2, FALSE));
	CHECK(!UnSetHooks(thread + 1));
	CHECK(removeCalls == 0 && ReadHook(&g_callHook)); // wrong owner cannot remove hooks

	CHECK(SetKeyboardFilterHook(TRUE) && SetMouseFilterHook(TRUE));
	KBDLLHOOKSTRUCT realKey = {};
	MSLLHOOKSTRUCT realMouse = {};
	CHECK(KeyboardProc(HC_ACTION, 0, (LPARAM)&realKey) == TRUE);
	CHECK(MouseProc(HC_ACTION, 0, (LPARAM)&realMouse) == TRUE);
	const int removalStart = removeCalls;
	failingRemove = WH_KEYBOARD_LL;
	CHECK(!UnSetHooks(thread));
	CHECK(removeCalls == removalStart + 5); // all removals attempted after first fails
	CHECK(ReadHook(&g_keyboardHook) && !ReadHook(&g_mouseHook) && !ReadHook(&g_callHook));
	CHECK(!ReadLong(&g_keyboardActive) && !ReadLong(&g_mouseActive));
	CHECK(KeyboardProc(HC_ACTION, 0, (LPARAM)&realKey) == 17); // physically installed, logically inactive
	CHECK(!ReadLong(&g_shared->threadId)); // disabled before cleanup
	failingRemove = -1;
	CHECK(UnSetHooks(thread));
	CHECK(!g_ownerThread && !ReadLong(&g_shared->processId));

	// Partial install failure must not leave a receiver published or leak hooks.
	failingInstall = WH_CALLWNDPROC;
	CHECK(!SetHooks(thread, WM_APP, 0, WM_APP + 2, FALSE));
	CHECK(!g_ownerThread && !ReadHook(&g_messageHook) && !ReadHook(&g_dialogHook));
	CHECK(!ReadLong(&g_shared->threadId));
	failingInstall = -1;
	CHECK(SetHooks(thread, WM_APP, 0, WM_APP + 2, FALSE));

	// A nested/parallel callback is skipped rather than waiting on itself.
	InterlockedExchange(&g_callback, 1);
	CHECK(!BeginCallback());
	CHECK(ReadLong(&g_shared->reentrantHints) == 1);
	InterlockedExchange(&g_callback, 0);
	CHECK(CallWndProc(-1, 0, 0) == 17);
	CHECK(CallWndProc(HC_ACTION, 0, 0) == 17);
	CHECK(GetMessageProc(HC_ACTION, PM_REMOVE, 0) == 17);
	CHECK(DialogMessageProc(0, 0, 0) == 17);
	CHECK(nextCalls == 5);
	HANDLE workers[8];
	for (int i = 0; i < 8; ++i) { workers[i] = CreateThread(NULL, 0, CallbackStress, NULL, 0, NULL); CHECK(workers[i]); }
	CHECK(WaitForMultipleObjects(8, workers, TRUE, 10000) == WAIT_OBJECT_0);
	for (int i = 0; i < 8; ++i) CloseHandle(workers[i]);
	CHECK(!ReadLong(&stressInside) && !ReadLong(&stressOverlap));

	postSucceeds = FALSE;
	for (int i = 0; i < 100; ++i) PostHint(WM_APP, 0, 0);
	CHECK(ReadLong(&g_shared->postFailures) == 100);
	VNCHOOKS_DIAGNOSTICS diagnostics = {};
	CHECK(!GetHookDiagnostics(&diagnostics));
	diagnostics.size = sizeof(diagnostics);
	CHECK(GetHookDiagnostics(&diagnostics));
	CHECK(diagnostics.version == 1 && diagnostics.postFailures == 100 && diagnostics.ownerThreadId == thread);
	CHECK(Coordinate(-9999999999LL) == -32768 && Coordinate(9999999999LL) == 32767);
	CHECK(Coordinate(-100) == -100 && Coordinate(100) == 100);
	CHECK(UnSetHooks(thread));

	// Input callbacks after removal or with invalid payload must pass through.
	CHECK(KeyboardProc(HC_ACTION, 0, 0) == 17 && MouseProc(HC_ACTION, 0, 0) == 17);
	CHECK(SetHooks(thread, WM_APP, 0, WM_APP + 2, TRUE));
	CHECK(ReadHook(&g_messageHook) && !ReadHook(&g_callHook) && !ReadHook(&g_dialogHook));
	CHECK(UnSetHooks(thread));
	CHECK(DllMain(GetModuleHandleA(NULL), DLL_PROCESS_DETACH, NULL));
	printf("PASS: inert loader, ownership, publication, partial failure, non-short-circuit cleanup, retry, reentrancy, invalid callbacks, bounded diagnostics and DDI hooks.\n");
	return 0;
}
