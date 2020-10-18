add_definitions(-D_USE_MATH_DEFINES)
add_executable(showmysky
                main.cpp
                util.cpp
                GLWidget.cpp
                MainWindow.cpp
                ToolsWidget.cpp
                Manipulator.cpp
                RadiancePlot.cpp
                AtmosphereRenderer.cpp
                ../common/util.cpp
                ../config.h)
target_link_libraries(showmysky Qt5::Core Qt5::Widgets Qt5::OpenGL)

install(TARGETS showmysky DESTINATION "${installBinDir}")
