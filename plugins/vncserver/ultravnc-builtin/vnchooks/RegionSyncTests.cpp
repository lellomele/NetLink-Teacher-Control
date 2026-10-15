// Copyright (c) 2025-26 prof. Ing. Raffaele Mele
#include "stdhdrs.h"
#include <stdio.h>
#include "vncRegionQueue.h"

// The production omni_mutex implementation is a recursive Win32 critical
// section. These two definitions avoid initializing the unrelated omni_thread
// subsystem (and its legacy TLS/worker infrastructure) in this isolated test.
omni_mutex::omni_mutex() { InitializeCriticalSection(&crit); }
omni_mutex::~omni_mutex() { DeleteCriticalSection(&crit); }
omni_thread::init_t::init_t() {}
omni_thread::init_t::~init_t() {}

struct Fixture {
	VncRegionQueue queue;
	rfb::Region2D received;
	HANDLE wake;
	volatile LONG stop;
};
struct Producer { Fixture *fixture; int row; };
static DWORD WINAPI Produce(LPVOID opaque) {
	Producer *producer = (Producer *)opaque;
	for (int i = 0; i < 4096; ++i) {
		const int x = (i % 64) * 2, y = producer->row * 2;
		producer->fixture->queue.add(rfb::Rect(x, y, x + 1, y + 1));
		SetEvent(producer->fixture->wake); // deliberately outside queue lock
	}
	return 0;
}
static DWORD WINAPI Consume(LPVOID opaque) {
	Fixture *fixture = (Fixture *)opaque;
	for (;;) {
		WaitForSingleObject(fixture->wake, 100);
		ResetEvent(fixture->wake);
		fixture->queue.drain(fixture->received);
		if (InterlockedCompareExchange(&fixture->stop, 0, 0)) break;
	}
	return 0;
}
#define CHECK(expr) do { if (!(expr)) { printf("FAIL line %d: %s\n", __LINE__, #expr); return __LINE__; } } while (0)

int main() {
	DWORD stabilized = 0, after = 0;
	for (int round = 0; round < 3; ++round) {
	{
		Fixture fixture;
		fixture.stop = 0;
		fixture.wake = CreateEventA(NULL, TRUE, FALSE, NULL);
		CHECK(fixture.wake);
		HANDLE consumer = CreateThread(NULL, 0, Consume, &fixture, 0, NULL);
		CHECK(consumer);
		HANDLE threads[4]; Producer producers[4];
		for (int i = 0; i < 4; ++i) {
			producers[i].fixture = &fixture; producers[i].row = i;
			threads[i] = CreateThread(NULL, 0, Produce, &producers[i], 0, NULL);
			CHECK(threads[i]);
		}
		CHECK(WaitForMultipleObjects(4, threads, TRUE, 10000) == WAIT_OBJECT_0);
		for (int i = 0; i < 4; ++i) CloseHandle(threads[i]);
		InterlockedExchange(&fixture.stop, 1); SetEvent(fixture.wake);
		CHECK(WaitForSingleObject(consumer, 10000) == WAIT_OBJECT_0);
		CloseHandle(consumer); CloseHandle(fixture.wake);
		fixture.queue.drain(fixture.received);
		rfb::Region2D expected;
		for (int row = 0; row < 4; ++row)
			for (int x = 0; x < 64; ++x) expected.assign_union(rfb::Rect(x * 2, row * 2, x * 2 + 1, row * 2 + 1));
		CHECK(fixture.received.equals(expected));
		rfb::Region2D empty;
		fixture.queue.drain(empty);
		CHECK(empty.is_empty());
	}
	GdiFlush();
	after = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
	// GDI caches deleted region handles per thread; compare repeated complete
	// lifecycles after warm-up, not cold zero against the populated OS cache.
	if (!round) stabilized = after;
	else CHECK(after <= stabilized + 1);
	}
	printf("PASS: 3 x 16384 updates, 4 concurrent producers + consumer, exact union/drain; stable GDI resources %lu -> %lu.\n", stabilized, after);
	return 0;
}
