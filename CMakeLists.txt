cmake_minimum_required(VERSION 3.10.0)
project(CalcMySky VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(${CMAKE_CXX_COMPILER_ID} MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=return-type -Wall -Wextra")
endif()

if(ADD_CXX_FLAGS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ADD_CXX_FLAGS}")
endif()
include(cmake/CheckGLM.cmake)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
find_package(Qt5 5.9 REQUIRED Core OpenGL)
find_package(Eigen3 REQUIRED)
include_directories(SYSTEM ${EIGEN3_INCLUDE_DIR})
include_directories(${CMAKE_BINARY_DIR})

if(WIN32)
    set(shadersDestPath "${CMAKE_INSTALL_PREFIX}/${CMAKE_PROJECT_NAME}")
    set(installBinDir "${CMAKE_INSTALL_PREFIX}/${CMAKE_PROJECT_NAME}")

    # Deploy Qt
    foreach(lib Core Widgets Gui)
        install(FILES "$<TARGET_FILE:Qt5::${lib}>" DESTINATION "${installBinDir}")
    endforeach()
    install(FILES "${Qt5Core_DIR}/../../../plugins/platforms/qwindows$<$<CONFIG:Debug>:d>.dll" DESTINATION "${installBinDir}/platforms")

else()
    include(GNUInstallDirs)
    set(shadersDestPath "${CMAKE_INSTALL_DATAROOTDIR}/${CMAKE_PROJECT_NAME}")
    set(installBinDir "${CMAKE_INSTALL_BINDIR}")
endif()

install(DIRECTORY "${CMAKE_SOURCE_DIR}/shaders" DESTINATION "${shadersDestPath}")

configure_file(config.h.in config.h)
add_subdirectory(CalcMySky)
add_subdirectory(ShowMySky)

enable_testing()
add_subdirectory(tests)
