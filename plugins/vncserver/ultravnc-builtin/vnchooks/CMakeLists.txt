set(vnchooks_SOURCES
	${ultravnc_DIR}/winvnc/vnchooks/VNCHooks.cpp
	${ultravnc_DIR}/winvnc/vnchooks/vnchooks.rc
)

add_library(vnchooks MODULE ${vnchooks_SOURCES})
set_source_files_properties(${vnchooks_SOURCES} PROPERTIES SKIP_UNITY_BUILD_INCLUSION TRUE)
set_default_target_properties(vnchooks)
set_target_properties(vnchooks PROPERTIES PREFIX "")
# Never inject a C/C++ runtime, emutls or winpthreads into another application.
# Only zero/constant-initialized POD and Win32 calls: no CRT startup is needed.
target_compile_options(vnchooks PRIVATE -fno-exceptions -fno-rtti
	-fno-threadsafe-statics -fno-use-cxa-atexit -fno-stack-protector)
if(VEYON_BUILD_WIN64)
	set(VNCHOOKS_ENTRY DllMain)
else()
	set(VNCHOOKS_ENTRY _DllMain@12)
endif()
set_target_properties(vnchooks PROPERTIES LINK_FLAGS
	"-nostdlib -Wl,--entry,${VNCHOOKS_ENTRY} -Wl,--exclude-all-symbols -Wl,--no-undefined")
target_link_libraries(vnchooks PRIVATE -lkernel32 -luser32)

if(WITH_TESTS)
	add_executable(netlink-hook-safety-tests HookSafetyTests.cpp)
	target_include_directories(netlink-hook-safety-tests PRIVATE ${ultravnc_DIR}/winvnc/vnchooks)
	target_compile_options(netlink-hook-safety-tests PRIVATE -Wall -Wextra -Werror -Wno-cast-function-type)
	target_link_options(netlink-hook-safety-tests PRIVATE -static-libgcc -static-libstdc++)
	target_link_libraries(netlink-hook-safety-tests PRIVATE -lkernel32 -luser32)
	add_test(NAME netlink-hook-safety COMMAND netlink-hook-safety-tests)
	add_test(NAME netlink-hook-loader COMMAND netlink-hook-safety-tests --load-only $<TARGET_FILE:vnchooks>)
	set_tests_properties(netlink-hook-safety netlink-hook-loader PROPERTIES TIMEOUT 30)
	add_executable(netlink-region-sync-tests RegionSyncTests.cpp ${ultravnc_DIR}/winvnc/winvnc/rfbRegion_win32.cpp)
	target_include_directories(netlink-region-sync-tests PRIVATE ${ultravnc_DIR}
		${ultravnc_DIR}/winvnc ${ultravnc_DIR}/winvnc/winvnc ${ultravnc_DIR}/winvnc/omnithread)
	target_compile_options(netlink-region-sync-tests PRIVATE -Wall -Wextra -Wno-unused-parameter)
	target_link_options(netlink-region-sync-tests PRIVATE -static-libgcc -static-libstdc++)
	target_link_libraries(netlink-region-sync-tests PRIVATE -lgdi32 -luser32 -lkernel32)
	add_test(NAME netlink-region-sync COMMAND netlink-region-sync-tests)
	set_tests_properties(netlink-region-sync PROPERTIES TIMEOUT 30)
endif()
