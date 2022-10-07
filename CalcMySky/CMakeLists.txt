add_executable(calcmysky
                main.cpp
                util.cpp
                glinit.cpp
                cmdline.cpp
                shaders.cpp
                interpolation-guides.cpp
                "${PROJECT_BINARY_DIR}/config.h")
target_compile_definitions(calcmysky PRIVATE -DSHOWMYSKY_COMPILING_CALCMYSKY)
target_link_libraries(calcmysky PUBLIC Qt${QT_VERSION}::Core Qt${QT_VERSION}::OpenGL Qt${QT_VERSION}::Widgets PRIVATE version common)

install(TARGETS calcmysky DESTINATION "${installBinDir}")
