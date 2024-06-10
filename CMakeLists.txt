cmake_minimum_required(VERSION 3.15)

project(vSID VERSION "0.8.0" LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release)

set(CMAKE_CXX_COMPILER clang-cl)
set(CMAKE_C_COMPILER clang-cl)

cmake_policy(SET CMP0091 NEW)
cmake_policy(SET CMP0156 NEW)

# directory for additional linkage
link_directories(${CMAKE_SOURCE_DIR}/include/es)

# directory where headers are stored
include_directories(${CMAKE_SOURCE_DIR}/include)

# DLL source files
set(SOURCES 
    area.cpp
    configparser.cpp
    flightplan.cpp
    messageHandler.cpp
    pch.cpp
    sid.cpp
    timeHandler.cpp
    utils.cpp
    vSID.cpp
    vSIDPlugin.cpp
)

# DLL target
add_library(vSID SHARED ${SOURCES})
target_compile_options(vSID PRIVATE -O2)

# set to use MSVC
set_property(TARGET vSID PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# link against ES library
target_link_libraries(vSID PRIVATE EuroScopePlugInDLL.lib)
