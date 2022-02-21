add_executable(calcmysky
                main.cpp
                util.cpp
                glinit.cpp
                cmdline.cpp
                shaders.cpp
                ../common/Spectrum.cpp
                ../common/AtmosphereParameters.cpp
                ../common/EclipsedDoubleScatteringPrecomputer.cpp
                ../common/util.cpp
                "${CMAKE_BINARY_DIR}/config.h")
target_compile_definitions(calcmysky PRIVATE -DSHOWMYSKY_COMPILING_CALCMYSKY)
target_link_libraries(calcmysky PUBLIC Qt5::Core Qt5::OpenGL PRIVATE version)

install(TARGETS calcmysky DESTINATION "${installBinDir}")
