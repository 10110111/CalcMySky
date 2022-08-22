add_executable(calcmysky
                main.cpp
                util.cpp
                glinit.cpp
                cmdline.cpp
                shaders.cpp
                interpolation-guides.cpp
                ../common/Spectrum.cpp
                ../common/AtmosphereParameters.cpp
                ../common/EclipsedDoubleScatteringPrecomputer.cpp
                ../common/util.cpp
                "${CMAKE_BINARY_DIR}/config.h")
target_compile_definitions(calcmysky PRIVATE -DSHOWMYSKY_COMPILING_CALCMYSKY)
target_link_libraries(calcmysky PUBLIC Qt${QT_VERSION}::Core Qt${QT_VERSION}::OpenGL Qt${QT_VERSION}::Widgets PRIVATE version)

install(TARGETS calcmysky DESTINATION "${installBinDir}")
