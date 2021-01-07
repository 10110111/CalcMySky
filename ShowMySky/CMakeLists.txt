add_definitions(-D_USE_MATH_DEFINES)
add_library(ShowMySky SHARED
             api/AtmosphereRenderer.cpp
             AtmosphereRenderer.cpp
             util.cpp
             ../common/EclipsedDoubleScatteringPrecomputer.cpp
             ../common/AtmosphereParameters.cpp
             ../common/Spectrum.cpp
             ../common/util.cpp
             ../config.h)
target_compile_definitions(ShowMySky PRIVATE -DSHOWMYSKY_COMPILING_SHARED_LIB)
target_link_libraries(ShowMySky Qt5::Core Qt5::OpenGL)

if(WIN32)
    # For some reason, showmysky target name conflicts with ShowMySky target,
    # even though on the filesystem they should result in names differing more
    # than by case: showmysky.exe and ShowMySky.dll
    set(showmyskyTarget showmysky.exe)
else()
    set(showmyskyTarget showmysky)
endif()
add_executable(${showmyskyTarget}
                main.cpp
                util.cpp
                GLWidget.cpp
                MainWindow.cpp
                ToolsWidget.cpp
                Manipulator.cpp
                RadiancePlot.cpp
                ../common/Spectrum.cpp
              )
target_link_libraries(${showmyskyTarget} Qt5::Core Qt5::Widgets Qt5::OpenGL)
if(WIN32)
    # Addition to the above hack: remove extraneous .exe
    set_target_properties(${showmyskyTarget} PROPERTIES SUFFIX "")
endif()

install(TARGETS ${showmyskyTarget} DESTINATION "${installBinDir}")
install(TARGETS ShowMySky DESTINATION "${installLibDir}")
install(FILES api/Exception.hpp api/AtmosphereRenderer.hpp api/Settings.hpp DESTINATION "${installIncDir}/ShowMySky")
