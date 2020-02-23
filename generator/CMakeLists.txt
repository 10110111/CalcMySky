add_executable(atmosphere-textures-generator
                main.cpp
                util.cpp
                glinit.cpp
                cmdline.cpp
                shaders.cpp
                ../config.h)
target_link_libraries(atmosphere-textures-generator Qt5::Core Qt5::OpenGL)
