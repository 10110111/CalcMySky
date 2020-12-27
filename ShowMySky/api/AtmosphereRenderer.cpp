#include "AtmosphereRenderer.hpp"
#include "../AtmosphereRenderer.hpp"

ShowMySky::AtmosphereRenderer* ShowMySky_AtmosphereRenderer_create(QOpenGLFunctions_3_3_Core* gl,
                                                                    QString const* pathToData,
                                                                    ShowMySky::Settings* tools)
{
    return new AtmosphereRenderer(*gl,*pathToData,tools);
}
