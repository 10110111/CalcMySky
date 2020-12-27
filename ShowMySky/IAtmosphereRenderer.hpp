#ifndef INCLUDE_ONCE_02040F35_0604_4759_A47D_71849AAC953D
#define INCLUDE_ONCE_02040F35_0604_4759_A47D_71849AAC953D

#include <memory>
#include <functional>

#include <QObject>
#include <qopengl.h>

#include "Settings.hpp"

class QString;
class QOpenGLShaderProgram;
class QOpenGLFunctions_3_3_Core;

namespace ShowMySky
{

class IAtmosphereRenderer
{
public:
    struct SpectralRadiance
    {
        std::vector<float> wavelengths; // nm
        std::vector<float> radiances;   // W/(m^2*sr*nm)
        // Direction from which this radiance was measured
        float azimuth;   // degrees
        float elevation; // degrees

        auto size() const { return wavelengths.size(); }
        bool empty() const { return wavelengths.empty(); }
    };

    struct Direction
    {
        float azimuth;
        float elevation;
    };
public:
    virtual void loadData(QByteArray viewDirVertShaderSrc, QByteArray viewDirFragShaderSrc,
                          std::function<void(QOpenGLShaderProgram&)> applyViewDirectionUniforms) = 0;
    virtual bool readyToRender() const = 0;
    virtual bool canGrabRadiance() const = 0;
    virtual GLuint getLuminanceTexture() = 0;
    virtual void draw() = 0;
    virtual void resizeEvent(int width, int height) = 0;
    virtual SpectralRadiance getPixelSpectralRadiance(QPoint const& pixelPos) const = 0;
    virtual Direction getViewDirection(QPoint const& pixelPos) const = 0;
    virtual QObject* asQObject() = 0;
    virtual ~IAtmosphereRenderer() = default;

    // Debug methods
    virtual void reloadShaders() = 0;
    virtual void setScattererEnabled(QString const& name, bool enable) = 0;

    // Signals
    virtual void loadProgress(QString const& currentActivity, int stepsDone, int stepsToDo) = 0;
};

}

extern "C"
{
ShowMySky::IAtmosphereRenderer*
    ShowMySky_AtmosphereRenderer_create(QOpenGLFunctions_3_3_Core* gl,
                                        QString const* pathToData,
                                        ShowMySky::Settings* tools);
}

#endif
