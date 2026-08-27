cmake_minimum_required(VERSION 3.15)

project(library-cpp-wrapper VERSION 2.0.0 LANGUAGES CXX)

# --------------------------------------------------------------------------
# Options
# --------------------------------------------------------------------------
option(BUILD_EXAMPLES "Build example programs" OFF)
option(BUILD_TESTS    "Build the test suite"   OFF)
option(FSW_HEADER_ONLY "Use header-only version of the library" OFF)

# --------------------------------------------------------------------------
# Core library
# --------------------------------------------------------------------------
if(FSW_HEADER_ONLY)
    # Header-only "interface" library: no .cpp to compile.
    add_library(library-cpp-wrapper INTERFACE)
    target_include_directories(library-cpp-wrapper
        INTERFACE
            ${CMAKE_CURRENT_SOURCE_DIR}/core
    )
    target_compile_features(library-cpp-wrapper INTERFACE cxx_std_17)
else()
    add_library(library-cpp-wrapper STATIC
        core/filemanager.cpp
    )
    target_include_directories(library-cpp-wrapper
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/core
    )
    target_compile_features(library-cpp-wrapper PUBLIC cxx_std_17)
    target_compile_options(library-cpp-wrapper PRIVATE
        -Wall -Wextra -Wpedantic
    )
endif()

# --------------------------------------------------------------------------
# Examples
# --------------------------------------------------------------------------
if(BUILD_EXAMPLES)
    set(EXAMPLES
        basic_usage
        advanced_features
    )
    foreach(example ${EXAMPLES})
        add_executable(${example} examples/${example}.cpp)
        target_link_libraries(${example} PRIVATE library-cpp-wrapper)
        if(FSW_HEADER_ONLY)
            target_compile_definitions(${example} PRIVATE FSW_HEADER_ONLY)
        endif()
    endforeach()
endif()

# --------------------------------------------------------------------------
# Tests
# --------------------------------------------------------------------------
if(BUILD_TESTS)
    enable_testing()
    add_executable(test_filemanager tests/test_filemanager.cpp)
    target_link_libraries(test_filemanager PRIVATE library-cpp-wrapper)
    add_test(NAME FileManagerTest COMMAND test_filemanager)
endif()

# --------------------------------------------------------------------------
# Installation
# --------------------------------------------------------------------------
install(TARGETS library-cpp-wrapper
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    EXPORT library-cpp-wrapper-targets
)

install(FILES
    core/filemanager.h
    core/filemanager.hpp
    core/filemanager_errors.h
    core/filemanager_glob.h
    DESTINATION include
)

install(EXPORT library-cpp-wrapper-targets
    FILE library-cpp-wrapper-config.cmake
    DESTINATION lib/cmake/library-cpp-wrapper
)

# Generate a version file once.
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/library-cpp-wrapper-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/library-cpp-wrapper-config-version.cmake"
    DESTINATION lib/cmake/library-cpp-wrapper
)
