#ifndef INCLUDE_ONCE_5DB905D2_61C0_44DB_8F35_67B31BD78315
#define INCLUDE_ONCE_5DB905D2_61C0_44DB_8F35_67B31BD78315

#include <cmath>
#include <array>
#include <deque>
#include <memory>
#include <glm/glm.hpp>
#include <QObject>
#include <QOpenGLTexture>
#include <QOpenGLShaderProgram>
#include <QOpenGLFunctions_3_3_Core>
#include "../common/types.hpp"

class ToolsWidget;
class AtmosphereRenderer : public QObject
{
    Q_OBJECT

    using ShaderProgPtr=std::unique_ptr<QOpenGLShaderProgram>;
    using TexturePtr=std::unique_ptr<QOpenGLTexture>;
    QOpenGLFunctions_3_3_Core& gl;
public:
    enum class DitheringMode
    {
        Disabled,    //!< Dithering disabled, will leave the infamous color bands
        Color565,    //!< 16-bit color (AKA High color) with R5_G6_B5 layout
        Color666,    //!< TN+film typical color depth in TrueColor mode
        Color888,    //!< 24-bit color (AKA True color)
        Color101010, //!< 30-bit color (AKA Deep color)
    };
    enum class DragMode
    {
        None,
        Sun,
        Camera,
    };

    struct Parameters
    {
        unsigned wavelengthSetCount=0;
        float atmosphereHeight=NAN;
        std::map<QString/*scatterer name*/,PhaseFunctionType> scatterers;
    };

    AtmosphereRenderer(QOpenGLFunctions_3_3_Core& gl, QString const& pathToData, Parameters const& params, ToolsWidget* tools);
    AtmosphereRenderer(AtmosphereRenderer const&)=delete;
    AtmosphereRenderer(AtmosphereRenderer&&)=delete;
    ~AtmosphereRenderer();

    void draw();
    void setDragMode(DragMode mode, int x=0, int y=0) { dragMode=mode; prevMouseX=x; prevMouseY=y; }
    void mouseMove(int x, int y);
    void resizeEvent(int width, int height);
    void setScattererEnabled(QString const& name, bool enable);
    void reloadShaders(QString const& pathToData);

private:
    ToolsWidget* tools;
    Parameters const& params;

    GLuint vao=0, vbo=0, fbo=0;
    std::vector<TexturePtr> transmittanceTextures;
    std::vector<TexturePtr> irradianceTextures;
    // Indexed as singleScatteringTextures[scattererName][wavelengthSetIndex]
    std::map<QString/*scattererName*/,std::vector<TexturePtr>> singleScatteringTextures;
    QOpenGLTexture multipleScatteringTexture;
    QOpenGLTexture bayerPatternTexture;
    QOpenGLTexture texFBO;

    std::vector<ShaderProgPtr> zeroOrderScatteringPrograms;
    // Indexed as singleScatteringPrograms[renderMode][scattererName][wavelengthSetIndex]
    std::vector<std::unique_ptr<std::map<QString/*scattererName*/,std::vector<ShaderProgPtr>>>> singleScatteringPrograms;
    ShaderProgPtr multipleScatteringProgram;
    ShaderProgPtr luminanceToScreenRGB;
    std::map<QString/*scattererName*/,bool> scatterersEnabledStates;

    DragMode dragMode=DragMode::None;
    int prevMouseX, prevMouseY;

    void parseParams(QString const& pathToData);
    void loadTextures(QString const& pathToData);
    void setupRenderTarget();
    void loadShaders(QString const& pathToData);
    void setupBuffers();

    QVector3D rgbMaxValue() const;
    void makeBayerPatternTexture();
    glm::ivec2 loadTexture2D(QString const& path);
    glm::ivec4 loadTexture4D(QString const& path);

    void renderZeroOrderScattering();
    void renderSingleScattering();
    void renderMultipleScattering();

signals:
    void needRedraw();
};

#endif
