if(NOT DEFINED LOIM_BINARY OR NOT EXISTS "${LOIM_BINARY}")
    message(FATAL_ERROR "LOIM_BINARY must point to an installed executable")
endif()
if(NOT DEFINED LOIM_STAGE_ROOT)
    message(FATAL_ERROR "LOIM_STAGE_ROOT is required")
endif()

file(REAL_PATH "${LOIM_STAGE_ROOT}" stage_root)
file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES "${LOIM_BINARY}"
    RESOLVED_DEPENDENCIES_VAR resolved_dependencies
    UNRESOLVED_DEPENDENCIES_VAR unresolved_dependencies
)

set(all_dependencies ${resolved_dependencies} ${unresolved_dependencies})
foreach(dependency IN LISTS all_dependencies)
    string(TOLOWER "${dependency}" dependency_lower)
    if(dependency_lower MATCHES "(^|[/\\])qt[0-9]" OR
       dependency_lower MATCHES "poppler")
        message(FATAL_ERROR "Forbidden Qt/Poppler runtime dependency: ${dependency}")
    endif()
endforeach()

foreach(dependency IN LISTS resolved_dependencies)
    file(REAL_PATH "${dependency}" dependency_path)
    string(TOLOWER "${dependency_path}" dependency_lower)
    string(TOLOWER "${stage_root}" stage_lower)
    string(FIND "${dependency_lower}" "${stage_lower}/" stage_prefix_position)

    if(stage_prefix_position EQUAL 0 OR dependency_lower STREQUAL stage_lower)
        message(FATAL_ERROR
            "Unexpected bundled dynamic dependency (register it before release): ${dependency}")
    endif()

    if(APPLE)
        if(NOT dependency_path MATCHES "^/System/Library/" AND
           NOT dependency_path MATCHES "^/usr/lib/")
            message(FATAL_ERROR "Non-system macOS dependency: ${dependency}")
        endif()
    elseif(WIN32)
        file(TO_CMAKE_PATH "$ENV{SystemRoot}" system_root)
        string(TOLOWER "${system_root}" system_root_lower)
        string(FIND "${dependency_lower}" "${system_root_lower}/" system_prefix_position)
        if(NOT system_prefix_position EQUAL 0)
            message(FATAL_ERROR "Non-system Windows dependency: ${dependency}")
        endif()
    elseif(UNIX)
        if(NOT dependency_path MATCHES "^/lib/" AND
           NOT dependency_path MATCHES "^/lib64/" AND
           NOT dependency_path MATCHES "^/usr/lib/" AND
           NOT dependency_path MATCHES "^/usr/lib64/")
            message(FATAL_ERROR "Non-system Linux dependency: ${dependency}")
        endif()
    endif()
endforeach()

list(LENGTH resolved_dependencies resolved_count)
list(LENGTH unresolved_dependencies unresolved_count)
message(STATUS
    "Runtime dependency audit passed: ${resolved_count} resolved, ${unresolved_count} unresolved")
