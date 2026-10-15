# Apply version-pinned vendor corrections without opaque binary dependencies.
# Idempotent: accept only pristine sources or the exact already-applied patch.
if(NOT GIT_FOUND)
    message(FATAL_ERROR "Git is required to validate the pinned vendor patch")
endif()
function(school_apply_vendor school_directory school_patch)
# Git ignores a ceiling equal to its current working directory. Use its parent
# so an unpacked source tree nested in another checkout cannot silently apply
# zero patches after discovering that unrelated ancestor repository.
get_filename_component(school_ceiling "${school_directory}" DIRECTORY)
execute_process(COMMAND "${CMAKE_COMMAND}" -E env "GIT_CEILING_DIRECTORIES=${school_ceiling}" "${GIT_EXECUTABLE}" apply --check "${school_patch}"
    WORKING_DIRECTORY "${school_directory}" RESULT_VARIABLE school_pristine
    OUTPUT_QUIET ERROR_QUIET)
if(school_pristine EQUAL 0)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env "GIT_CEILING_DIRECTORIES=${school_ceiling}" "${GIT_EXECUTABLE}" apply "${school_patch}"
        WORKING_DIRECTORY "${school_directory}" RESULT_VARIABLE school_applied)
    if(NOT school_applied EQUAL 0)
        message(FATAL_ERROR "Could not apply hardening patch ${school_patch}")
    endif()
else()
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env "GIT_CEILING_DIRECTORIES=${school_ceiling}" "${GIT_EXECUTABLE}" apply --reverse --check "${school_patch}"
        WORKING_DIRECTORY "${school_directory}" RESULT_VARIABLE school_already_applied
        OUTPUT_QUIET ERROR_QUIET)
    if(NOT school_already_applied EQUAL 0)
        message(FATAL_ERROR "Sources do not match pinned hardening patch ${school_patch}")
    endif()
endif()
endfunction()
school_apply_vendor("${ultravnc_DIR}" "${CMAKE_SOURCE_DIR}/patches-ultravnc.diff")
school_apply_vendor("${libvncserver_DIR}" "${CMAKE_SOURCE_DIR}/patches-libvncserver.diff")
school_apply_vendor("${qthttpserver_DIR}" "${CMAKE_SOURCE_DIR}/patches-qthttpserver.diff")
