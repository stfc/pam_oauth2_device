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

# dirname of PAM library
string(REGEX MATCH "^.*/" PAM_LIBRARY_PATH ${PAM_LIBRARY})
message(DEBUG "Pam library path ${PAM_LIBRARY_PATH}")

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
# GNU dirs provide default prefix /usr/local which won't work as install location
# for the library as /usr/local/.../secure is not searched when locating PAM mods
# see also #7
include(GNUInstallDirs)

# If a configuration directory and file already exist, prefer those over a default
find_path(PAM_CONFIG_INSTALL_DIR config.json
        PATHS "${CMAKE_INSTALL_SYSCONFDIR}/pam_oauth2_device"
                /etc/pam_oauth2_device
                /usr/local/etc/pam_oauth2_device OPTIONAL)
if(PAM_CONFIG_INSTALL_DIR)
    message(DEBUG "Existing config directory ${PAM_CONFIG_INSTALL_DIR} found")
else()
    set(PAM_CONFIG_INSTALL_DIR "/etc/pam_oauth2_device" DOC "Install location for configuration file")
    message(DEBUG "No config dir found, using ${PAM_CONFIG_INSTALL_DIR}")
endif()

# Try to locate the correct location to install the PAM module
# Observationally, it seems to be in "security" under the location of the PAM library

find_path(PAM_MODULE_DIR NAMES pam_access.so pam_deny.so pam_permit.so pam_env.so
        PATHS   "${PAM_LIBRARY_PATH}/security"
                "${CMAKE_INSTALL_LIBDIR}/security"
                "/usr/lib64/security"                               # RedHat flavoured systems
                "/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}/security"   # Debian flavour
        DOC "location of system PAM modules"
        REQUIRED
)

message(DEBUG "Installing module into ${PAM_MODULE_DIR}")

install(TARGETS pam_oauth2_device
    LIBRARY DESTINATION "${PAM_MODULE_DIR}"
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

# Only install template if a configuration file does not exist! (re-fix #14)
if(EXISTS "${PAM_CONFIG_INSTALL_DIR}/config.json")
    message(DEBUG "Not overwriting existing config ${PAM_CONFIG_INSTALL_DIR}/config.json")
else()
    install(FILES config_template.json
        DESTINATION "${PAM_CONFIG_INSTALL_DIR}"
        RENAME config.json
        PERMISSIONS OWNER_READ OWNER_WRITE
        COMPONENT config
    )
endif ()

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
