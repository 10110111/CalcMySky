#include "ShowMySky/AtmosphereRenderer.hpp"
#include "../AtmosphereRenderer.hpp"

constexpr quint32 abi=ShowMySky_ABI_version;
#undef ShowMySky_ABI_version
extern "C"
{
extern SHOWMYSKY_DLL_PUBLIC const quint32 ShowMySky_ABI_version;
const quint32 ShowMySky_ABI_version=abi;
}

ShowMySky::AtmosphereRenderer* ShowMySky_AtmosphereRenderer_create(QOpenGLFunctions_3_3_Core* gl,
                                                                    QString const* pathToData,
                                                                    ShowMySky::Settings* tools,
                                                                    std::function<void(QOpenGLShaderProgram&)> const* drawSurface)
{
    return new AtmosphereRenderer(*gl,*pathToData,tools,*drawSurface);
}
