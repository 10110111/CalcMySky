cmake_minimum_required(VERSION 3.10.0)
if(POLICY CMP0110)
    cmake_policy(SET CMP0110 OLD)
endif()

set(staticProjectVersion 0.0.6)
project(CalcMySky VERSION ${staticProjectVersion} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(${CMAKE_CXX_COMPILER_ID} MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "-Werror=return-type -Wall -Wextra -fvisibility=hidden ${CMAKE_CXX_FLAGS}")
endif()

if(MSVC)
    set(CMAKE_CXX_FLAGS "/wd4267 /wd4305 /wd4244 ${CMAKE_CXX_FLAGS}")
endif()
include(cmake/CheckGLM.cmake)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

set(DEFAULT_QT_VERSION 5)
set(ALT_QT_VERSION 6)
if(NOT DEFINED QT_VERSION)
    message("** NOTE: QT_VERSION is not defined, using default setup: Qt${DEFAULT_QT_VERSION}. Pass -DQT_VERSION=${ALT_QT_VERSION} to cmake to\n    choose Qt${ALT_QT_VERSION} or -DQT_VERSION=${DEFAULT_QT_VERSION} to get rid of this message and choose Qt${DEFAULT_QT_VERSION}.")
    set(QT_VERSION ${DEFAULT_QT_VERSION})
endif()

if("${QT_VERSION}" STREQUAL 5)
    find_package(Qt5 5.12 REQUIRED Core OpenGL)
elseif("${QT_VERSION}" STREQUAL 6)
    find_package(Qt6 6.0 REQUIRED Core OpenGL Widgets OpenGLWidgets)
else()
    message(FATAL_ERROR "QT_VERSION must be either 5 or 6")
endif()

IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    # macOS architectures: x86_64 or arm64
    IF(NOT CMAKE_OSX_ARCHITECTURES)
        # find utilite 'lipo'
        FIND_PROGRAM(LIPO_COMMAND lipo)
        IF(LIPO_COMMAND)
            MESSAGE(STATUS "Found lipo: ${LIPO_COMMAND}")
            GET_TARGET_PROPERTY(QMAKE_LOCATION Qt${QT_VERSION}::qmake LOCATION)
            # get architecture of Qt framework
            EXECUTE_PROCESS(COMMAND ${LIPO_COMMAND} -archs ${QMAKE_LOCATION} TIMEOUT 30 OUTPUT_VARIABLE QT_ARCHS OUTPUT_STRIP_TRAILING_WHITESPACE)
            STRING(REPLACE " " ";" QT_ARCHS ${QT_ARCHS})
            SET(CMAKE_OSX_ARCHITECTURES "${QT_ARCHS}" CACHE STRING "macOS architectures" FORCE)
        ELSE()
            # current system's processor architecture
            SET(CMAKE_OSX_ARCHITECTURES "${CMAKE_HOST_SYSTEM_PROCESSOR}" CACHE STRING "macOS architectures" FORCE)
        ENDIF()
    ENDIF()
    # macOS deployment targets
    IF(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        IF("${QT_VERSION}" STREQUAL 6)
            # Qt6: by default - universal binary + minimal operating system is macOS Big Sur
            SET(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS deployment version" FORCE)
        ELSE()
            SET(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS deployment version" FORCE)
        ENDIF()
    ENDIF()
ENDIF()

find_package(Eigen3 REQUIRED)
include_directories(SYSTEM ${EIGEN3_INCLUDE_DIR})
include_directories(${CMAKE_BINARY_DIR})

if(WIN32)
    set(shadersDestPath "${CMAKE_PROJECT_NAME}")
    set(installBinDir "${CMAKE_PROJECT_NAME}")
    set(installLibDir "${installBinDir}")
    set(installConfDir "${CMAKE_PROJECT_NAME}/lib")
    set(installIncDir "${CMAKE_PROJECT_NAME}/include")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHs")

    # Deploy Qt
    if(${QT_VERSION} STREQUAL 6)
        set(libsToInstall Core;Widgets;Gui;OpenGL;OpenGLWidgets)
    else()
        set(libsToInstall Core;Widgets;Gui)
    endif()
    foreach(lib ${libsToInstall})
        install(FILES "$<TARGET_FILE:Qt${QT_VERSION}::${lib}>" DESTINATION "${installBinDir}")
    endforeach()
    install(FILES "${Qt${QT_VERSION}Core_DIR}/../../../plugins/platforms/qwindows$<$<CONFIG:Debug>:d>.dll" DESTINATION "${installBinDir}/platforms")

else()
    include(GNUInstallDirs)
    set(shadersDestPath "${CMAKE_INSTALL_DATAROOTDIR}/${CMAKE_PROJECT_NAME}")
    set(installBinDir "${CMAKE_INSTALL_BINDIR}")
    set(installLibDir "${CMAKE_INSTALL_LIBDIR}")
    set(installConfDir "${CMAKE_INSTALL_LIBDIR}")
    set(installIncDir "${CMAKE_INSTALL_INCLUDEDIR}")
endif()

install(DIRECTORY "${CMAKE_SOURCE_DIR}/shaders" DESTINATION "${shadersDestPath}")

# The first command is a dummy, it doesn't actually create its output: it's
# only needed to force re-running of version.sh for the case when we've changed
# the version but didn't touch other dependencies.
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/check version update" COMMAND true)
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/version.cpp"
                   DEPENDS "${CMAKE_BINARY_DIR}/check version update"
                   COMMAND "${CMAKE_SOURCE_DIR}/version.sh" "${staticProjectVersion}" "${CMAKE_SOURCE_DIR}/version.cpp.in" "${CMAKE_BINARY_DIR}/version.cpp")
add_library(version STATIC "${CMAKE_BINARY_DIR}/version.cpp")
add_library(common STATIC
             common/EclipsedDoubleScatteringPrecomputer.cpp
             common/AtmosphereParameters.cpp
             common/Spectrum.cpp
             common/util.cpp)
target_link_libraries(common PUBLIC Qt${QT_VERSION}::Core Qt${QT_VERSION}::OpenGL Qt${QT_VERSION}::Widgets)

configure_file(config.h.in config.h)
add_subdirectory(CalcMySky)
add_subdirectory(ShowMySky)
add_subdirectory(doc)

enable_testing()
add_subdirectory(tests)
