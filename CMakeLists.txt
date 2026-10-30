cmake_minimum_required(VERSION 3.25)

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
# (if a non-default directory exists but is empty, it is ignored)
#
# Note the config file may have mode 600 (particularly if it contains the secrets inside
# it, as opposed to putting the secrets in a separate file), and find_path will
# fail to recognise it if cmake is run as non-root, and when install runs sudo,
# the config file will then be overwritten.  find_path will also fail if the
# directory is 700.  Other tools like EXISTS or even TIMESTAMP also fail.

# Where to put the configuration template - but do not overwrite an existing configuration
foreach( candidate_abs_path /etc/pam_oauth2_device
		/usr/local/etc/pam_oauth2_device
		"${CMAKE_INSTALL_SYSCONFDIR}/pam_oauth2_device" )
    if(IS_DIRECTORY "${candidate_abs_path}")
    	set(PAM_CONFIG_INSTALL_DIR ${candidate_abs_path} )
    	break()
    endif()
endforeach()

if(PAM_CONFIG_INSTALL_DIR)
    message(DEBUG "Existing config directory ${PAM_CONFIG_INSTALL_DIR} found")
else()
    set(PAM_CONFIG_INSTALL_DIR "/etc/pam_oauth2_device")
    message(DEBUG "No config dir found, using ${PAM_CONFIG_INSTALL_DIR}")
endif()

# Try to locate the correct location to install the PAM module
# Observationally, it seems to be in "security" under the location of the PAM library

find_path(PAM_MODULE_DIR NAMES pam_access.so pam_deny.so pam_permit.so pam_env.so
        PATHS   "${PAM_LIBRARY_PATH}/security"                      # Should cover any distro
                "/usr/lib64/security"                               # RedHat flavoured systems
                "/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}/security"   # Debian flavour
                "${CMAKE_INSTALL_LIBDIR}/security"
        DOC "location of system PAM modules"
        REQUIRED
)

message(DEBUG "Installing module into ${PAM_MODULE_DIR}")

install(TARGETS pam_oauth2_device
    LIBRARY DESTINATION "${PAM_MODULE_DIR}"
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

# Slightly clunky hack. At install time, the config file location should be writeable
# IS_WRITABLE is the more accurate check but is not available before 3.29
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.29")
    install(CODE "if(IS_WRITABLE ${PAM_CONFIG_INSTALL_DIR} AND NOT EXISTS \"${PAM_CONFIG_INSTALL_DIR}/config.json\")
        file(COPY \"${CMAKE_SOURCE_DIR}/config_template.json\" DESTINATION \"${PAM_CONFIG_INSTALL_DIR}\")
        file(RENAME \"${PAM_CONFIG_INSTALL_DIR}/config_template.json\"  \"${PAM_CONFIG_INSTALL_DIR}/config.json\")
        message(INFO \"Created template file ${PAM_CONFIG_INSTALL_DIR}/config.json\")
    endif()")
else()
    install(CODE "if(EXISTS ${PAM_CONFIG_INSTALL_DIR} AND NOT EXISTS \"${PAM_CONFIG_INSTALL_DIR}/config.json\")
        file(COPY \"${CMAKE_SOURCE_DIR}/config_template.json\" DESTINATION \"${PAM_CONFIG_INSTALL_DIR}\")
        file(RENAME \"${PAM_CONFIG_INSTALL_DIR}/config_template.json\"  \"${PAM_CONFIG_INSTALL_DIR}/config.json\")
        message(INFO \"Created template file ${PAM_CONFIG_INSTALL_DIR}/config.json\")
    endif()")
endif()

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
