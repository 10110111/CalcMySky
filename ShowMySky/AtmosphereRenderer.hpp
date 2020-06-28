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
    using ScattererName=QString;
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
        std::vector<unsigned> wavelengths;
        unsigned wavelengthSetCount=0;
        unsigned eclipseSingleScatteringTextureSizeForCosVZA=0;
        unsigned eclipseSingleScatteringTextureSizeForRelAzimuth=0;
        float atmosphereHeight=NAN;
        float earthRadius=NAN;
        float earthMoonDistance=NAN;
        std::map<QString/*scatterer name*/,PhaseFunctionType> scatterers;
    };

    struct SpectralRadiance
    {
        std::vector<float> wavelengths; // nm
        std::vector<float> radiances;   // W/(m^2*sr*nm)
        auto size() const { return wavelengths.size(); }
        bool empty() const { return wavelengths.empty(); }
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
    void reloadShaders();
    SpectralRadiance getPixelSpectralRadiance(QPoint const& pixelPos) const;

private:
    ToolsWidget* tools;
    Parameters const& params;
    QString pathToData;

    GLuint vao=0, vbo=0, mainFBO=0;
    GLuint eclipseSingleScatteringPrecomputationFBO=0;
    std::vector<TexturePtr> multipleScatteringTextures;
    std::vector<TexturePtr> transmittanceTextures;
    std::vector<TexturePtr> irradianceTextures;
    std::vector<GLuint> radianceRenderBuffers;
    // Indexed as singleScatteringTextures[scattererName][wavelengthSetIndex]
    std::map<ScattererName,std::vector<TexturePtr>> singleScatteringTextures;
    std::map<ScattererName,std::vector<TexturePtr>> eclipsedSingleScatteringPrecomputationTextures;
    QOpenGLTexture bayerPatternTexture;
    QOpenGLTexture mainFBOTexture;
    QSize viewportSize;

    std::vector<ShaderProgPtr> zeroOrderScatteringPrograms;
    std::vector<ShaderProgPtr> multipleScatteringPrograms;
    // Indexed as singleScatteringPrograms[renderMode][scattererName][wavelengthSetIndex]
    using ScatteringProgramsMap=std::map<ScattererName,std::vector<ShaderProgPtr>>;
    std::vector<std::unique_ptr<ScatteringProgramsMap>> singleScatteringPrograms;
    std::vector<std::unique_ptr<ScatteringProgramsMap>> eclipsedSingleScatteringPrograms;
    // Indexed as eclipsedSingleScatteringPrecomputationPrograms[scattererName][wavelengthSetIndex]
    std::unique_ptr<ScatteringProgramsMap> eclipsedSingleScatteringPrecomputationPrograms;
    ShaderProgPtr luminanceToScreenRGB;
    std::map<ScattererName,bool> scatterersEnabledStates;

    DragMode dragMode=DragMode::None;
    int prevMouseX, prevMouseY;

    void parseParams();
    void loadTextures();
    void reloadScatteringTextures();
    void setupRenderTarget();
    void loadShaders();
    void setupBuffers();

    double moonAngularRadius() const;
    double cameraMoonDistance() const;
    glm::dvec3 sunDirection() const;
    glm::dvec3 moonPosition() const;
    glm::dvec3 moonPositionRelativeToSunAzimuth() const;
    glm::dvec3 cameraPosition() const;
    QVector3D rgbMaxValue() const;
    void makeBayerPatternTexture();
    glm::ivec2 loadTexture2D(QString const& path);
    void loadTexture4D(QString const& path);

    void precomputeEclipsedSingleScattering();
    void renderZeroOrderScattering();
    void renderSingleScattering();
    void renderMultipleScattering();
    void clearRadianceFrames();
    bool canGrabRadiance() const;

signals:
    void needRedraw();
};

#endif
