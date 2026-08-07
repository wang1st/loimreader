if(NOT DEFINED SOURCE_DIR OR NOT DEFINED PATCH_FILE)
    message(FATAL_ERROR "SOURCE_DIR and PATCH_FILE are required")
endif()

execute_process(
    COMMAND git apply --ignore-space-change --whitespace=nowarn --check "${PATCH_FILE}"
    WORKING_DIRECTORY "${SOURCE_DIR}"
    RESULT_VARIABLE check_result
    OUTPUT_QUIET
    ERROR_QUIET)
if(check_result EQUAL 0)
    execute_process(
        COMMAND git apply --ignore-space-change --whitespace=nowarn "${PATCH_FILE}"
        WORKING_DIRECTORY "${SOURCE_DIR}"
        RESULT_VARIABLE apply_result)
    if(NOT apply_result EQUAL 0)
        message(FATAL_ERROR "Failed to apply SDL_ttf compatibility patch")
    endif()
    return()
endif()

execute_process(
    COMMAND git apply --ignore-space-change --whitespace=nowarn --reverse --check "${PATCH_FILE}"
    WORKING_DIRECTORY "${SOURCE_DIR}"
    RESULT_VARIABLE reverse_result
    OUTPUT_QUIET
    ERROR_QUIET)
if(NOT reverse_result EQUAL 0)
    message(FATAL_ERROR "SDL_ttf source is neither patched nor patchable")
endif()
