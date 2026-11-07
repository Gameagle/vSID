cmake_minimum_required(VERSION 3.20)
project(vsid_tz_tests CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

add_executable(vsid_tz_tests tzif_tests.cpp ../tzif.cpp)
target_include_directories(vsid_tz_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

add_test(NAME vsid_tz_tests COMMAND vsid_tz_tests)

# Differential test against std::chrono's tz database; needs a zoneinfo folder, run manually:
#   vsid_tz_diff <zoneinfo-folder> [--verbose]
add_executable(vsid_tz_diff tz_diff.cpp ../tzif.cpp)
target_include_directories(vsid_tz_diff PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
