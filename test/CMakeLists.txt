# Test executables

# test_config
add_executable(test_config
    test_config.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/include/config.cpp
)
target_include_directories(test_config PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/include
)
target_link_libraries(test_config PRIVATE
    GTest::gtest
    GTest::gtest_main
    nlohmann_json::nlohmann_json
    Threads::Threads
)
add_test(NAME test_config COMMAND test_config)
set_tests_properties(test_config PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# unit
add_executable(unit
    unit.cpp
    temp_file.cpp
    $<TARGET_OBJECTS:pam_oauth2_device_core>
)
target_include_directories(unit PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/include/nayuki
)
target_link_libraries(unit PRIVATE
    pam_oauth2_device_core
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)
add_test(NAME unit COMMAND unit)
set_tests_properties(unit PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# test_pam_oauth2_device (requires mock server on port 8042)
add_executable(test_pam_oauth2_device
    test_pam_oauth2_device.cpp
    $<TARGET_OBJECTS:pam_oauth2_device_core>
)
target_include_directories(test_pam_oauth2_device PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/include
)
target_link_libraries(test_pam_oauth2_device PRIVATE
    pam_oauth2_device_core
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)
add_test(NAME test_pam_oauth2_device COMMAND test_pam_oauth2_device)
set_tests_properties(test_pam_oauth2_device PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# Convenience target: check
add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_config unit test_pam_oauth2_device
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
