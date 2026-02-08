cmake_minimum_required(VERSION 3.14)
project(cpp_core VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Create header-only library (INTERFACE library)
add_library(cpp_core INTERFACE)

# Set target properties
set_target_properties(cpp_core PROPERTIES
    VERSION ${PROJECT_VERSION}
)

# Set include directories
target_include_directories(cpp_core INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Optional: Set up installation
include(GNUInstallDirs)

install(TARGETS cpp_core
    EXPORT cpp_coreTargets
)

install(EXPORT cpp_coreTargets
    FILE cpp_coreTargets.cmake
    NAMESPACE cpp_core::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpp_core
)

