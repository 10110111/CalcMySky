add_executable(calcmysky
                main.cpp
                util.cpp
                glinit.cpp
                cmdline.cpp
                shaders.cpp
                Spectrum.cpp
                ../common/util.cpp
                ../config.h)
target_link_libraries(calcmysky Qt5::Core Qt5::OpenGL)

install(TARGETS calcmysky DESTINATION "${installBinDir}")
