cmake_minimum_required(VERSION 3.16)

project(pam_oauth2_device
    VERSION 1.05
    DESCRIPTION "PAM module for OAuth 2.0 Device flow"
    HOMEPAGE_URL "https://github.com/stfc/pam_oauth2_device"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

add_definitions(-D_GNU_SOURCE)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_compile_options(-Wall)

# Dependencies
# 1. Threads
find_package(Threads REQUIRED)

# 2. CURL
find_package(CURL REQUIRED)

# 3. PAM
find_path(PAM_INCLUDE_DIR NAMES security/pam_appl.h)
find_library(PAM_LIBRARY NAMES pam)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PAM
    REQUIRED_VARS PAM_LIBRARY PAM_INCLUDE_DIR
)
if(PAM_FOUND AND NOT TARGET PAM::PAM)
    add_library(PAM::PAM UNKNOWN IMPORTED)
    set_target_properties(PAM::PAM PROPERTIES
        IMPORTED_LOCATION "${PAM_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${PAM_INCLUDE_DIR}"
    )
endif()

# 4. LDAP & LBER
find_path(LDAP_INCLUDE_DIR NAMES ldap.h)
find_library(LDAP_LIBRARY NAMES ldap)
find_library(LBER_LIBRARY NAMES lber)
find_package_handle_standard_args(LDAP
    REQUIRED_VARS LDAP_LIBRARY LBER_LIBRARY LDAP_INCLUDE_DIR
)
if(LDAP_FOUND AND NOT TARGET LDAP::LDAP)
    add_library(LDAP::LDAP UNKNOWN IMPORTED)
    set_target_properties(LDAP::LDAP PROPERTIES
        IMPORTED_LOCATION "${LDAP_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LDAP_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES "${LBER_LIBRARY}"
    )
endif()

# 5. nlohmann_json (CMake config or fallback to system headers)
find_package(nlohmann_json 3.2.0 QUIET CONFIG)
if(NOT TARGET nlohmann_json::nlohmann_json)
    find_path(NLOHMANN_JSON_INCLUDE_DIR NAMES nlohmann/json.hpp REQUIRED)
    add_library(nlohmann_json::nlohmann_json INTERFACE IMPORTED)
    set_target_properties(nlohmann_json::nlohmann_json PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${NLOHMANN_JSON_INCLUDE_DIR}"
    )
endif()

# Core object library containing the implementation files
add_library(pam_oauth2_device_core OBJECT
    src/pam_oauth2_device.cpp
    src/include/config.cpp
    src/include/metadata.cpp
    src/include/pam_oauth2_curl.cpp
    src/include/pam_oauth2_log.cpp
    src/include/ldapquery.c
    src/include/nayuki/BitBuffer.cpp
    src/include/nayuki/QrCode.cpp
    src/include/nayuki/QrSegment.cpp
)

target_include_directories(pam_oauth2_device_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/include/nayuki>
)

target_link_libraries(pam_oauth2_device_core
    PUBLIC
        PAM::PAM
        CURL::libcurl
        LDAP::LDAP
        nlohmann_json::nlohmann_json
        Threads::Threads
)

# Shared PAM module (pam_oauth2_device.so)
add_library(pam_oauth2_device MODULE
    $<TARGET_OBJECTS:pam_oauth2_device_core>
)

target_link_libraries(pam_oauth2_device PRIVATE pam_oauth2_device_core)

set_target_properties(pam_oauth2_device PROPERTIES
    PREFIX ""
    OUTPUT_NAME "pam_oauth2_device"
)

# Optional utilities
option(BUILD_UTILS "Build utility programs" OFF)
if(BUILD_UTILS)
    add_executable(tls-debug util/tls-debug/tls-debug.c)
    target_link_libraries(tls-debug PRIVATE CURL::libcurl)
endif()

# Installation
include(GNUInstallDirs)

set(PAM_MODULE_DIR "${CMAKE_INSTALL_LIBDIR}/security" CACHE PATH "Installation directory for PAM modules")

install(TARGETS pam_oauth2_device
    LIBRARY DESTINATION "${PAM_MODULE_DIR}"
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

#install(FILES config_template.json
#    DESTINATION "${CMAKE_INSTALL_SYSCONFDIR}/pam_oauth2_device"
#    RENAME config.json
#    PERMISSIONS OWNER_READ OWNER_WRITE
#)

# Testing
include(CTest)
if(BUILD_TESTING)
    find_package(GTest)
    if(GTest_FOUND)
        add_subdirectory(test)
    else()
        message(STATUS "Google Test not found; skipping tests.")
    endif()
endif()
