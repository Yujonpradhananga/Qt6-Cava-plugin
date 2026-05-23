cmake_minimum_required(VERSION 3.20)
project(CavaMonitor)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Ensure the QML plugin can find libcavamonitor.so in the same install directory
# at runtime, regardless of whether it's in the Nix store or ~/.local.
set(CMAKE_INSTALL_RPATH "$ORIGIN")
set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
set(CMAKE_SKIP_BUILD_RPATH OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Qml)
find_package(PkgConfig REQUIRED)
pkg_check_modules(Pipewire REQUIRED IMPORTED_TARGET libpipewire-0.3)
pkg_check_modules(Cava QUIET IMPORTED_TARGET cava)
if(NOT Cava_FOUND)
    message(STATUS "Cava pkg-config not found, trying manual detection...")
    
    find_library(CAVA_LIBRARY 
        NAMES cava libcava
        PATHS /usr/lib /usr/local/lib /usr/lib64 /usr/local/lib64
        PATH_SUFFIXES cava
    )
    
    find_path(CAVA_INCLUDE_DIR 
        NAMES cava/cavacore.h
        PATHS /usr/include /usr/local/include
    )
    
    if(CAVA_LIBRARY AND CAVA_INCLUDE_DIR)
        message(STATUS "Found Cava library: ${CAVA_LIBRARY}")
        message(STATUS "Found Cava includes: ${CAVA_INCLUDE_DIR}")
        set(Cava_FOUND TRUE)
    else()
        message(FATAL_ERROR "Could not find Cava library. Please install it from https://github.com/karlstav/cava")
    endif()
endif()

set(QML_IMPORT_PATH "${CMAKE_INSTALL_PREFIX}" CACHE STRING "" FORCE)

qt_add_qml_module(cavamonitor
    URI CavaMonitor
    VERSION 1.0
    SOURCES
        cavamonitor.cpp
        cavamonitor.hpp
    NO_PLUGIN_OPTIONAL
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/CavaMonitor"
)
target_link_libraries(cavamonitor PRIVATE
    Qt6::Core
    Qt6::Qml
    PkgConfig::Pipewire
)
if(TARGET PkgConfig::Cava)
    target_link_libraries(cavamonitor PRIVATE PkgConfig::Cava)
else()
    target_include_directories(cavamonitor PRIVATE ${CAVA_INCLUDE_DIR})
    target_link_libraries(cavamonitor PRIVATE ${CAVA_LIBRARY})
endif()
install(TARGETS cavamonitor cavamonitorplugin
    LIBRARY DESTINATION CavaMonitor
    RUNTIME DESTINATION CavaMonitor
)

install(FILES 
    ${CMAKE_CURRENT_BINARY_DIR}/CavaMonitor/qmldir
    DESTINATION CavaMonitor
)
