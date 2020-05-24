add_definitions(-D_USE_MATH_DEFINES)
add_executable(atmosphere-textures-preview
                main.cpp
                util.cpp
                GLWidget.cpp
                ToolsWidget.cpp
                Manipulator.cpp
                AtmosphereRenderer.cpp
                ../common/util.cpp
                ../config.h)
target_link_libraries(atmosphere-textures-preview Qt5::Core Qt5::Widgets Qt5::OpenGL)

install(TARGETS atmosphere-textures-preview DESTINATION "${installBinDir}")
