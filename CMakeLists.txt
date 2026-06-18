cmake_minimum_required(VERSION 3.20)
project(CavaMonitor)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

set(CMAKE_INSTALL_RPATH "$ORIGIN")
set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
set(CMAKE_SKIP_BUILD_RPATH OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Qml)
find_package(PkgConfig REQUIRED)
pkg_check_modules(Pipewire REQUIRED IMPORTED_TARGET libpipewire-0.3)
pkg_check_modules(FFTW3 REQUIRED IMPORTED_TARGET fftw3)

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

# compile cavacore.c as a plain C file alongside the plugin
target_sources(cavamonitor PRIVATE cavacore.c)
target_include_directories(cavamonitor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(cavamonitor PRIVATE
    Qt6::Core
    Qt6::Qml
    PkgConfig::Pipewire
    PkgConfig::FFTW3
)

install(TARGETS cavamonitor cavamonitorplugin
    LIBRARY DESTINATION CavaMonitor
    RUNTIME DESTINATION CavaMonitor
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/CavaMonitor/qmldir
    DESTINATION CavaMonitor
)
