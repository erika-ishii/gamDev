# Project Settings
cmake_minimum_required(VERSION 3.20...3.30 FATAL_ERROR)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT MyGame)

project(giraphics VERSION 0.1 LANGUAGES C CXX)
# Enable /MP for all targets globally
if(MSVC)
    add_compile_options(/MP)
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Where extra cmake modules live
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# Import third-party deps (e.g., glfw) if the helper exists
if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/cmake/ImportDependencies.cmake")
  include(ImportDependencies)
  importDependencies()
endif()

# Subprojects
add_subdirectory(Engine)

# Optional test/setup module (only include if present)
include(BuildTest OPTIONAL)   # or include(BuildTests OPTIONAL) if that's the filename


add_subdirectory(Sandbox)

