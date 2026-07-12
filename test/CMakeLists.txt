include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

macro(add_recon_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE recon_core GTest::gtest_main)
    add_test(NAME ${name} COMMAND ${name})
endmacro()

add_recon_test(test_trade_store test_trade_store.cpp)
add_recon_test(test_matching_engine test_matching_engine.cpp)
add_recon_test(test_exception_workflow test_exception_workflow.cpp)
add_recon_test(test_settlement test_settlement.cpp)
add_recon_test(test_fix_handler test_fix_handler.cpp)
add_recon_test(test_reporting test_reporting.cpp)
add_recon_test(test_integration test_integration.cpp)
add_recon_test(test_fuzz test_fuzz.cpp)
