include(BuildVeyonPlugin)

option(WITH_INTERCEPTION "Enable optional external kernel input driver integration" OFF)
if(WITH_INTERCEPTION)
    add_library(interception STATIC ${CMAKE_SOURCE_DIR}/3rdparty/interception-source/library/interception.c)
    target_compile_definitions(interception PUBLIC INTERCEPTION_STATIC)
    target_include_directories(interception PUBLIC ${CMAKE_SOURCE_DIR}/3rdparty/interception-source/library)
endif()

build_veyon_plugin(windows-platform
	WindowsPlatformPlugin.cpp
	WindowsPlatformConfigurationPage.h
	WindowsPlatformConfigurationPage.cpp
	WindowsPlatformConfigurationPage.ui
	WindowsCoreFunctions.cpp
	WindowsFilesystemFunctions.cpp
	WindowsInputDeviceFunctions.cpp
	WindowsKeyboardShortcutTrapper.cpp
	WindowsNetworkFunctions.cpp
	WindowsServiceControl.cpp
	WindowsServiceCore.cpp
	WindowsServiceFunctions.cpp
	WindowsSessionFunctions.cpp
	WindowsUserFunctions.cpp
	WtsSessionManager.cpp
	DesktopInputController.cpp
	SasEventListener.cpp
	XEventLog.cpp
	${ultravnc_DIR}/winvnc/winvnc/vnckeymap.cpp
	${ultravnc_DIR}/addon/ms-logon/authSSP/authSSP.cpp
	${ultravnc_DIR}/addon/ms-logon/authSSP/GenClientServerContext.cpp
	WindowsPlatformConfiguration.h
	WindowsPlatformPlugin.h
	WindowsCoreFunctions.h
	WindowsFilesystemFunctions.h
	WindowsInputDeviceFunctions.h
	WindowsKeyboardShortcutTrapper.h
	WindowsNetworkFunctions.h
	WindowsServiceControl.h
	WindowsServicePolicy.h
	WindowsServiceCore.h
	WindowsServiceFunctions.h
	WindowsSessionFunctions.h
	WindowsUserFunctions.h
	WtsSessionManager.h
	DesktopInputController.h
	SasEventListener.h
	../common/LogonHelper.h
	../common/LogonHelper.cpp
	../common/PersistentLogonCredentials.h
	../common/PersistentLogonCredentials.cpp
	../common/PlatformSessionManager.h
	../common/PlatformSessionManager.cpp
	../common/ServiceDataManager.h
	../common/ServiceDataManager.cpp
	XEventLog.h
	windows.qrc
	)

target_include_directories(windows-platform PRIVATE
	../common
	${ultravnc_DIR}/addon/ms-logon/authSSP
	${ultravnc_DIR}/winvnc/winvnc
	${ultravnc_DIR}
	)

target_link_libraries(windows-platform PRIVATE -lws2_32 -lwtsapi32 -lnetapi32 -luserenv -liphlpapi)
if(WITH_INTERCEPTION)
    target_link_libraries(windows-platform PRIVATE interception)
    target_compile_definitions(windows-platform PRIVATE VEYON_WITH_INTERCEPTION)
endif()
target_link_libraries(windows-platform PRIVATE Qt${QT_MAJOR_VERSION}::GuiPrivate)
target_compile_definitions(windows-platform PRIVATE ULTRAVNC_VEYON_SUPPORT)
target_compile_options(windows-platform PRIVATE "-Wno-unknown-pragmas")
set_source_files_properties(WindowsNetworkFunctions.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION TRUE SKIP_PRECOMPILE_HEADERS TRUE)
