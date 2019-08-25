cmake_minimum_required(VERSION 3.10.0)
project(atmosphere-textures-generator VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)

include(CheckIncludeFileCXX)
check_include_file_cxx("glm/glm.hpp" HAVE_GLM)
if(NOT HAVE_GLM)
    unset(HAVE_GLM CACHE)
    message(FATAL_ERROR "GLM was not found")
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
find_package(Qt5 5.10 REQUIRED Core OpenGL)
configure_file(config.h.in config.h)
include_directories(${CMAKE_BINARY_DIR})
add_executable(atmosphere-textures-generator main.cpp config.h)
target_link_libraries(atmosphere-textures-generator Qt5::Core Qt5::OpenGL)
