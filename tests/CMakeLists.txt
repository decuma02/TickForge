# ==============================================================================
# Tests CMakeLists.txt (Google Test)
# ==============================================================================
include(GoogleTest)

# Unit Test Binary
add_executable(unit_tests
    unit/test_engine.cpp
)

target_link_libraries(unit_tests
    PRIVATE
        tickforge_core
        GTest::gtest
        GTest::gtest_main
)

configure_target_sanitizers(unit_tests)
gtest_discover_tests(unit_tests)

# Integration Test Binary
add_executable(integration_tests
    integration/test_integration.cpp
)

target_link_libraries(integration_tests
    PRIVATE
        tickforge_core
        GTest::gtest
        GTest::gtest_main
)

configure_target_sanitizers(integration_tests)
gtest_discover_tests(integration_tests)
