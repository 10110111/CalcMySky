if(WIN32)
    set(extraSrc win32res.rc)
endif()

add_definitions(-D_USE_MATH_DEFINES)
add_library(ShowMySky SHARED
             api/AtmosphereRenderer.cpp
             AtmosphereRenderer.cpp
             util.cpp
             "${CMAKE_BINARY_DIR}/config.h")
set_target_properties(ShowMySky PROPERTIES VERSION ${CMAKE_PROJECT_VERSION}
                      SOVERSION ${PROJECT_VERSION_MAJOR})
target_compile_definitions(ShowMySky PRIVATE -DSHOWMYSKY_COMPILING_SHARED_LIB)
target_link_libraries(ShowMySky PUBLIC Qt${QT_VERSION}::Core Qt${QT_VERSION}::OpenGL PRIVATE version common)

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
              )
target_link_libraries(${showmyskyTarget} PUBLIC Qt${QT_VERSION}::Core Qt${QT_VERSION}::Widgets Qt${QT_VERSION}::OpenGL PRIVATE version common)
if(${QT_VERSION} STREQUAL 6)
    target_link_libraries(${showmyskyTarget} PUBLIC Qt${QT_VERSION}::OpenGLWidgets)
endif()

set_target_properties(${showmyskyTarget} PROPERTIES OUTPUT_NAME showmysky)
if(WIN32)
    # Default subsystem is console. We don't want the console window to appear, so tweak linker parameters.
    set_target_properties(${showmyskyTarget} PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
endif()

install(TARGETS ${showmyskyTarget} DESTINATION "${installBinDir}")
install(TARGETS ShowMySky
        EXPORT ShowMySkyConfig
        LIBRARY DESTINATION "${installLibDir}"
        RUNTIME DESTINATION "${installLibDir}"
        ARCHIVE DESTINATION "${installLibDir}"
        INCLUDES DESTINATION "${installIncDir}"
        )
export(TARGETS ShowMySky NAMESPACE ShowMySky:: FILE "${CMAKE_CURRENT_BINARY_DIR}/ShowMySkyConfig.cmake")
install(EXPORT ShowMySkyConfig NAMESPACE ShowMySky:: DESTINATION "${installConfDir}/ShowMySky/cmake")
install(FILES api/Exception.hpp api/AtmosphereRenderer.hpp api/Settings.hpp DESTINATION "${installIncDir}/ShowMySky")
