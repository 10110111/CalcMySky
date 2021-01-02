#include "AtmosphereRenderer.hpp"
#include "../AtmosphereRenderer.hpp"

ShowMySky::AtmosphereRenderer* ShowMySky_AtmosphereRenderer_create(QOpenGLFunctions_3_3_Core* gl,
                                                                    QString const* pathToData,
                                                                    ShowMySky::Settings* tools,
                                                                    std::function<void(QOpenGLShaderProgram&)> const* drawSurface)
{
    return new AtmosphereRenderer(*gl,*pathToData,tools,*drawSurface);
}
