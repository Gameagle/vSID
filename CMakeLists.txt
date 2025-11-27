cmake_minimum_required(VERSION 3.15)

project(vSID VERSION "0.8.0" LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
cmake_policy(SET CMP0091 NEW)
cmake_policy(SET CMP0156 NEW)

# ensure 32-bit (Win32) build (ES limitation)
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "vSID must be built as 32-bit. Configure with -A Win32.")
endif()

# force static CRT for entire project
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "" FORCE)
endif()

# --- configure libcurl (external/curl provided by workflow) ---
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)

# Use Windows native SSL, no OpenSSL DLLs
set(CURL_USE_SCHANNEL ON CACHE BOOL "" FORCE)
set(CURL_USE_OPENSSL OFF CACHE BOOL "" FORCE)

# Only HTTP/HTTPS – no other protocols
set(HTTP_ONLY ON CACHE BOOL "" FORCE)

# Do NOT require libpsl
set(CURL_USE_LIBPSL OFF CACHE BOOL "" FORCE)

# ---- Strip unused features ----
# No alternative services, HSTS, cookies etc.
set(CURL_DISABLE_ALTSVC ON CACHE BOOL "" FORCE)
set(CURL_DISABLE_HSTS ON CACHE BOOL "" FORCE)
set(CURL_DISABLE_COOKIES ON CACHE BOOL "" FORCE)

# No protocols beyond HTTP(S)
set(CURL_DISABLE_WEBSOCKETS ON CACHE BOOL "" FORCE)
set(CURL_DISABLE_MQTT ON CACHE BOOL "" FORCE)
set(CURL_DISABLE_SMB ON CACHE BOOL "" FORCE)
set(CURL_DISABLE_RTSP ON CACHE BOOL "" FORCE)
set(CURL_DISABLE_DOH ON CACHE BOOL "" FORCE)   # no DNS-over-HTTPS
set(CURL_DISABLE_IPFS ON CACHE BOOL "" FORCE)

# No mail / legacy / misc protocols
set(CURL_DISABLE_IMAP ON CACHE BOOL "" FORCE)
set(CURL_DISABLE_POP3 ON CACHE BOOL "" FORCE)
set(CURL_DISABLE_SMTP ON CACHE BOOL "" FORCE)
set(CURL_DISABLE_DICT ON CACHE BOOL "" FORCE)
set(CURL_DISABLE_GOPHER ON CACHE BOOL "" FORCE)
set(CURL_DISABLE_TELNET ON CACHE BOOL "" FORCE)
set(CURL_DISABLE_TFTP ON CACHE BOOL "" FORCE)
set(CURL_DISABLE_LDAP ON CACHE BOOL "" FORCE)
set(CURL_DISABLE_LDAPS ON CACHE BOOL "" FORCE)

# No forms/MIME APIs
set(CURL_DISABLE_MIME ON CACHE BOOL "" FORCE)
set(CURL_DISABLE_FORM_API ON CACHE BOOL "" FORCE)

# No progress meter / verbose strings / netrc / date parsing
set(CURL_DISABLE_PROGRESS_METER ON CACHE BOOL "" FORCE)
set(CURL_DISABLE_VERBOSE_STRINGS ON CACHE BOOL "" FORCE)
set(CURL_DISABLE_NETRC ON CACHE BOOL "" FORCE)
set(CURL_DISABLE_PARSEDATE ON CACHE BOOL "" FORCE)

# auth methods
set(CURL_DISABLE_BASIC_AUTH ON CACHE BOOL "" FORCE)
set(CURL_DISABLE_BEARER_AUTH ON CACHE BOOL "" FORCE)
set(CURL_DISABLE_DIGEST_AUTH ON CACHE BOOL "" FORCE)
set(CURL_DISABLE_KERBEROS_AUTH ON CACHE BOOL "" FORCE)
set(CURL_DISABLE_NEGOTIATE_AUTH ON CACHE BOOL "" FORCE)
set(CURL_DISABLE_AWS ON CACHE BOOL "" FORCE)

add_subdirectory(${CMAKE_SOURCE_DIR}/external/curl)

# directory for additional linkage
link_directories(${CMAKE_SOURCE_DIR}/include/es)

# directory where headers are stored
include_directories(${CMAKE_SOURCE_DIR}/include)

# DLL source files
set(SOURCES 
    area.cpp
    configparser.cpp
    eseparser.cpp
    display.cpp
    flightplan.cpp
    menu.cpp
    messageHandler.cpp
    pch.cpp
    sid.cpp
    timeHandler.cpp
    utils.cpp
    versionchecker.cpp
    vSID.cpp
    vSIDPlugin.cpp
)

# DLL target
add_library(vSID SHARED ${SOURCES})
target_compile_options(vSID PRIVATE -O2)

# link curl statically
target_compile_definitions(vSID PRIVATE CURL_STATICLIB)

# NEW: choose the correct curl target and link it, plus ES lib
# Some curl CMake setups export CURL::libcurl, others just libcurl
if(TARGET CURL::libcurl)
    set(_curl_target CURL::libcurl)
elseif(TARGET libcurl)
    set(_curl_target libcurl)
else()
    message(FATAL_ERROR "libcurl target not found after add_subdirectory(external/curl)")
endif()

target_link_libraries(vSID
    PRIVATE
        ${_curl_target}
        EuroScopePlugInDLL.lib
)
