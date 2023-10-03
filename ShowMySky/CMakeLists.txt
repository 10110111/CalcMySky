if(WIN32)
    set(extraSrc win32res.rc)
endif()

add_definitions(-D_USE_MATH_DEFINES)
add_library(ShowMySky SHARED
             api/AtmosphereRenderer.cpp
             AtmosphereRenderer.cpp
             util.cpp
             "${PROJECT_BINARY_DIR}/config.h")
file(READ api/ShowMySky/AtmosphereRenderer.hpp rendererHeader)
string(REGEX MATCH "#define ShowMySky_ABI_version [0-9]+\n" abiVersionLine "${rendererHeader}")
string(REGEX REPLACE "#define ShowMySky_ABI_version ([0-9]+)\n" "\\1" abiVersion "${abiVersionLine}")
message(STATUS "ABI version: ${abiVersion}")
set_target_properties(ShowMySky PROPERTIES VERSION ${abiVersion}.0.0 SOVERSION ${abiVersion})
target_compile_definitions(ShowMySky PRIVATE -DSHOWMYSKY_COMPILING_SHARED_LIB)
target_link_libraries(ShowMySky PUBLIC Qt${QT_VERSION}::Core
	Qt${QT_VERSION}::OpenGL PRIVATE version common glm::glm)
set_target_properties(ShowMySky PROPERTIES OUTPUT_NAME ShowMySky-Qt${QT_VERSION})

add_library(ShowMySky::ShowMySky ALIAS ShowMySky)
target_include_directories(ShowMySky PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/api>)

set(showmyskyTarget showmysky-cmd)

if(${QT_VERSION} STREQUAL 5)
    qt5_add_resources(RES_SOURCES resources.qrc)
else(${QT_VERSION} STREQUAL 6)
    qt6_add_resources(RES_SOURCES resources.qrc)
endif()
add_executable(${showmyskyTarget}
               ${RES_SOURCES}
               ${extraSrc}
                main.cpp
                util.cpp
                GLWidget.cpp
                MainWindow.cpp
                ToolsWidget.cpp
                Manipulator.cpp
                RadiancePlot.cpp
                DockScrollArea.cpp
                GLSLCosineQualityChecker.cpp
              )
target_link_libraries(${showmyskyTarget} PUBLIC Qt${QT_VERSION}::Core
	Qt${QT_VERSION}::Widgets Qt${QT_VERSION}::OpenGL PRIVATE version common
	glm::glm)
if(${QT_VERSION} STREQUAL 6)
    target_link_libraries(${showmyskyTarget} PUBLIC Qt${QT_VERSION}::OpenGLWidgets)
endif()

set_target_properties(${showmyskyTarget} PROPERTIES OUTPUT_NAME showmysky)
if(WIN32 AND (NOT MINGW))
    # Default subsystem is console. We don't want the console window to appear, so tweak linker parameters.
    set_target_properties(${showmyskyTarget} PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
endif()

install(TARGETS ${showmyskyTarget} DESTINATION "${installBinDir}")
install(TARGETS ShowMySky
        EXPORT ShowMySky-Qt${QT_VERSION}Config
        LIBRARY DESTINATION "${installLibDir}"
        RUNTIME DESTINATION "${installLibDir}"
        ARCHIVE DESTINATION "${installLibDir}"
        INCLUDES DESTINATION "${installIncDir}"
        )
export(TARGETS ShowMySky NAMESPACE ShowMySky:: FILE "${CMAKE_CURRENT_BINARY_DIR}/ShowMySky-Qt${QT_VERSION}Config.cmake")
install(EXPORT ShowMySky-Qt${QT_VERSION}Config NAMESPACE ShowMySky:: DESTINATION "${installConfDir}/cmake/ShowMySky-Qt${QT_VERSION}")
install(FILES api/ShowMySky/Exception.hpp api/ShowMySky/AtmosphereRenderer.hpp api/ShowMySky/Settings.hpp DESTINATION "${installIncDir}/ShowMySky")
