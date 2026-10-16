if(NOT EXISTS "${APP}")
    message(FATAL_ERROR "Missing production executable")
endif()
file(STRINGS "${APP}" test_hooks REGEX
    "--(smoke-test|export-smoke-test|async-clear-smoke-test|persist-smoke-test|window-icon-smoke-test|hardening-test|verify-entitlement-test)")
if(test_hooks)
    message(FATAL_ERROR "Internal test entry points leaked into production executable")
endif()
execute_process(COMMAND "${APP}" --hardening-test
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 10)
if(NOT result MATCHES "^[1-9][0-9]*$" OR NOT error MATCHES "Unsupported option")
    message(FATAL_ERROR "Production executable did not reject internal test command: ${result} ${output} ${error}")
endif()
