# BuildVeyonApplication.cmake - Copyright (c) 2019-2022 Tobias Junghans
#
# description: build Veyon application
# usage: build_veyon_application(<NAME> <SOURCES>)

macro(build_veyon_application APPLICATION_NAME)
	if(VEYON_BUILD_ANDROID)
		add_library(${APPLICATION_NAME} SHARED ${ARGN})
	else()
		add_executable(${APPLICATION_NAME} ${ARGN})
		install(TARGETS ${APPLICATION_NAME} RUNTIME DESTINATION bin)
	endif()
	target_include_directories(${APPLICATION_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src)
	set_target_properties(${APPLICATION_NAME} PROPERTIES COMPILE_OPTIONS "${CMAKE_COMPILE_OPTIONS_PIE}")
	set_target_properties(${APPLICATION_NAME} PROPERTIES LINK_OPTIONS "${CMAKE_LINK_OPTIONS_PIE}")
	target_link_libraries(${APPLICATION_NAME} PRIVATE veyon-core)
	set_default_target_properties(${APPLICATION_NAME})
	# Preserve target/library/protocol identities; only Windows EXE filenames
	# change. Companion already has its final NetLink name.
	if(VEYON_BUILD_WINDOWS AND "${APPLICATION_NAME}" MATCHES "^veyon-(master|configurator|service|server|worker|cli|wcli)$")
		string(REGEX REPLACE "^veyon-" "netlink-" NETLINK_OUTPUT_NAME "${APPLICATION_NAME}")
		set_target_properties(${APPLICATION_NAME} PROPERTIES OUTPUT_NAME "${NETLINK_OUTPUT_NAME}")
	endif()
	if(WITH_PCH)
		target_precompile_headers(${APPLICATION_NAME} REUSE_FROM veyon-application-pch)
	endif()
endmacro()

