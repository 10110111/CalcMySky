if(WIN32)
    set(extraSrc win32res.rc)
endif()

add_definitions(-D_USE_MATH_DEFINES)
add_library(ShowMySky SHARED
             api/AtmosphereRenderer.cpp
             AtmosphereRenderer.cpp
             util.cpp
             ../common/EclipsedDoubleScatteringPrecomputer.cpp
             ../common/AtmosphereParameters.cpp
             ../common/Spectrum.cpp
             ../common/util.cpp
             "${CMAKE_BINARY_DIR}/config.h")
target_compile_definitions(ShowMySky PRIVATE -DSHOWMYSKY_COMPILING_SHARED_LIB)
target_link_libraries(ShowMySky PUBLIC Qt5::Core Qt5::OpenGL PRIVATE version)

if(WIN32)
    # For some reason, showmysky target name conflicts with ShowMySky target,
    # even though on the filesystem they should result in names differing more
    # than by case: showmysky.exe and ShowMySky.dll
    set(showmyskyTarget showmysky.exe)
else()
    set(showmyskyTarget showmysky)
endif()
qt5_add_resources(RES_SOURCES resources.qrc)
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
                ../common/Spectrum.cpp
              )
target_link_libraries(${showmyskyTarget} PUBLIC Qt5::Core Qt5::Widgets Qt5::OpenGL PRIVATE version)
if(WIN32)
    # Addition to the above hack: remove extraneous .exe
    set_target_properties(${showmyskyTarget} PROPERTIES SUFFIX "")
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
