cmake_minimum_required(VERSION 3.10.0)
project(atmosphere-textures-generator VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(${CMAKE_CXX_COMPILER_ID} MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=return-type -Wall -Wextra")
endif()

include(GNUInstallDirs)
if(ADD_CXX_FLAGS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ADD_CXX_FLAGS}")
endif()
include(cmake/CheckGLM.cmake)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
find_package(Qt5 5.9 REQUIRED Core OpenGL)
configure_file(config.h.in config.h)
include_directories(${CMAKE_BINARY_DIR})

add_subdirectory(generator)
