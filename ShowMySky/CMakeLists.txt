if(WIN32)
    set(extraSrc win32res.rc)
endif()

add_definitions(-D_USE_MATH_DEFINES)
add_library(ShowMySky-Qt${QT_VERSION} SHARED
             api/AtmosphereRenderer.cpp
             AtmosphereRenderer.cpp
             util.cpp
             "${CMAKE_BINARY_DIR}/config.h")
target_compile_definitions(ShowMySky-Qt${QT_VERSION} PRIVATE -DSHOWMYSKY_COMPILING_SHARED_LIB)
target_link_libraries(ShowMySky-Qt${QT_VERSION} PUBLIC Qt${QT_VERSION}::Core Qt${QT_VERSION}::OpenGL PRIVATE version common)

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
install(TARGETS ShowMySky-Qt${QT_VERSION}
        EXPORT ShowMySky-Qt${QT_VERSION}Config
        LIBRARY DESTINATION "${installLibDir}"
        RUNTIME DESTINATION "${installLibDir}"
        ARCHIVE DESTINATION "${installLibDir}"
        INCLUDES DESTINATION "${installIncDir}"
        )
export(TARGETS ShowMySky-Qt${QT_VERSION} NAMESPACE ShowMySky:: FILE "${CMAKE_CURRENT_BINARY_DIR}/ShowMySky-Qt${QT_VERSION}Config.cmake")
install(EXPORT ShowMySky-Qt${QT_VERSION}Config NAMESPACE ShowMySky:: DESTINATION "${installConfDir}/ShowMySky-Qt${QT_VERSION}/cmake")
install(FILES api/Exception.hpp api/AtmosphereRenderer.hpp api/Settings.hpp DESTINATION "${installIncDir}/ShowMySky")
