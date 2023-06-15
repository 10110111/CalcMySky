cmake_minimum_required(VERSION 3.10.0)
if(POLICY CMP0110)
    cmake_policy(SET CMP0110 OLD)
endif()

set(staticProjectVersion 0.3.1)
project(CalcMySky VERSION ${staticProjectVersion} LANGUAGES CXX)
set(CMAKE_FIND_USE_PACKAGE_REGISTRY false)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(${CMAKE_CXX_COMPILER_ID} MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "-Werror=return-type -Wall -Wextra -fvisibility=hidden ${CMAKE_CXX_FLAGS}")
endif()

if(MSVC)
    set(CMAKE_CXX_FLAGS "/wd4267 /wd4305 /wd4244 ${CMAKE_CXX_FLAGS}")
endif()
include(cmake/CPM.cmake)
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

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    # macOS architectures: x86_64 or arm64
    if(NOT CMAKE_OSX_ARCHITECTURES)
        # Find 'lipo' utility
        find_program(LIPO_COMMAND lipo)
        if(LIPO_COMMAND)
            message(STATUS "Found lipo: ${LIPO_COMMAND}")
            get_target_property(QMAKE_LOCATION Qt${QT_VERSION}::qmake LOCATION)
            # Get architecture of Qt
            execute_process(COMMAND ${LIPO_COMMAND} -archs ${QMAKE_LOCATION} TIMEOUT 30 OUTPUT_VARIABLE QT_ARCHS OUTPUT_STRIP_TRAILING_WHITESPACE)
            string(REPLACE " " ";" QT_ARCHS ${QT_ARCHS})
            set(CMAKE_OSX_ARCHITECTURES "${QT_ARCHS}" CACHE STRING "macOS architectures" FORCE)
        else()
            # If we can't find architecture of Qt, use that of the host system's CPU
            set(CMAKE_OSX_ARCHITECTURES "${CMAKE_HOST_SYSTEM_PROCESSOR}" CACHE STRING "macOS architectures" FORCE)
        endif()
    endif()
    # macOS deployment targets
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        if("${QT_VERSION}" STREQUAL 6)
            # Qt6: by default - universal binary + minimal operating system is macOS Big Sur
            set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS deployment version" FORCE)
        else()
            set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS deployment version" FORCE)
        endif()
    endif()
endif()

CPMFindPackage(NAME Eigen3
	URL https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.bz2
	URL_HASH SHA256=b4c198460eba6f28d34894e3a5710998818515104d6e74e5cc331ce31e46e626
	EXCLUDE_FROM_ALL yes
	DOWNLOAD_ONLY yes
	OPTIONS "EIGEN_BUILD_DOC no" "BUILD_TESTING no")
if(Eigen3_ADDED)
	# Eigen3's CMakeLists.txt works, but is very heavy
	add_library(Eigen3::Eigen IMPORTED INTERFACE)
	target_include_directories(Eigen3::Eigen INTERFACE ${Eigen3_SOURCE_DIR})
endif()
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
# only needed to force re-running of gen_version.cmake for the case when we've changed
# the version but didn't touch other dependencies.
add_custom_command(OUTPUT "${PROJECT_BINARY_DIR}/check version update" COMMAND $<IF:$<BOOL:${WIN32}>,echo.,true>)
add_custom_command(OUTPUT "${PROJECT_BINARY_DIR}/version.cpp"
	DEPENDS "${PROJECT_BINARY_DIR}/check version update"
	WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
	COMMAND "${CMAKE_COMMAND}"
		-D "staticVersion=${staticProjectVersion}"
		-D "inputFile=${PROJECT_SOURCE_DIR}/version.cpp.in"
		-D "outputFile=${PROJECT_BINARY_DIR}/version.cpp"
		-P "${PROJECT_SOURCE_DIR}/cmake/gen_version.cmake")
add_library(version STATIC "${PROJECT_BINARY_DIR}/version.cpp")
add_library(common STATIC
             common/EclipsedDoubleScatteringPrecomputer.cpp
             common/TextureAverageComputer.cpp
             common/AtmosphereParameters.cpp
             common/Spectrum.cpp
             common/util.cpp)
target_link_libraries(common PUBLIC Qt${QT_VERSION}::Core
	Qt${QT_VERSION}::OpenGL Qt${QT_VERSION}::Widgets PRIVATE glm::glm
	Eigen3::Eigen)

configure_file(config.h.in config.h)
add_subdirectory(CalcMySky)
add_subdirectory(ShowMySky)
add_subdirectory(doc)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	enable_testing()
	add_subdirectory(tests)
endif()
