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
#include "../common/AtmosphereParameters.hpp"

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

    AtmosphereRenderer(QOpenGLFunctions_3_3_Core& gl, QString const& pathToData, AtmosphereParameters const& params, ToolsWidget* tools);
    AtmosphereRenderer(AtmosphereRenderer const&)=delete;
    AtmosphereRenderer(AtmosphereRenderer&&)=delete;
    ~AtmosphereRenderer();
    void loadData();
    bool readyToRender() const { return readyToRender_; }

    void draw();
    void setDragMode(DragMode mode, int x=0, int y=0) { dragMode_=mode; prevMouseX_=x; prevMouseY_=y; }
    void mouseMove(int x, int y);
    void resizeEvent(int width, int height);
    void setScattererEnabled(QString const& name, bool enable);
    void reloadShaders();
    SpectralRadiance getPixelSpectralRadiance(QPoint const& pixelPos) const;
    Direction getViewDirection(QPoint const& pixelPos) const;

signals:
    void loadProgress(QString const& currentActivity, int stepsDone, int stepsToDo);

private:
    ToolsWidget* tools_;
    AtmosphereParameters params_;
    QString pathToData_;
    int totalLoadingStepsToDo_=-1, loadingStepsDone_=0;
    QString currentActivity_;

    GLuint vao_=0, vbo_=0, mainFBO_=0, viewDirectionFBO_=0;
    GLuint eclipseSingleScatteringPrecomputationFBO_=0;
    GLuint eclipseDoubleScatteringPrecomputationFBO_=0;
    // Lower and upper altitude slices from the 4D texture
    std::vector<TexturePtr> eclipsedDoubleScatteringTexturesLower_, eclipsedDoubleScatteringTexturesUpper_;
    std::vector<TexturePtr> multipleScatteringTextures_;
    std::vector<TexturePtr> transmittanceTextures_;
    std::vector<TexturePtr> irradianceTextures_;
    std::vector<GLuint> radianceRenderBuffers_;
    GLuint viewDirectionRenderBuffer_=0;
    // Indexed as singleScatteringTextures_[scattererName][wavelengthSetIndex]
    std::map<ScattererName,std::vector<TexturePtr>> singleScatteringTextures_;
    std::map<ScattererName,std::vector<TexturePtr>> eclipsedSingleScatteringPrecomputationTextures_;
    TexturePtr eclipsedDoubleScatteringPrecomputationScratchTexture_;
    std::vector<TexturePtr> eclipsedDoubleScatteringPrecomputationTargetTextures_;
    QOpenGLTexture bayerPatternTexture_;
    QOpenGLTexture mainFBOTexture_;
    QSize viewportSize_;
    float loadedAltitudeURTexCoordRange_[2]={NAN,NAN};
    float loadedEclipsedDoubleScatteringAltitudeURTexCoordRange_[2]={NAN,NAN};
    float staticAltitudeTexCoord_=-1;
    float eclipsedDoubleScatteringAltitudeAlphaUpper_=-1;

    std::vector<ShaderProgPtr> zeroOrderScatteringPrograms_;
    std::vector<ShaderProgPtr> eclipsedZeroOrderScatteringPrograms_;
    std::vector<ShaderProgPtr> multipleScatteringPrograms_;
    // Indexed as singleScatteringPrograms_[renderMode][scattererName][wavelengthSetIndex]
    using ScatteringProgramsMap=std::map<ScattererName,std::vector<ShaderProgPtr>>;
    std::vector<std::unique_ptr<ScatteringProgramsMap>> singleScatteringPrograms_;
    std::vector<std::unique_ptr<ScatteringProgramsMap>> eclipsedSingleScatteringPrograms_;
    std::vector<ShaderProgPtr> eclipsedDoubleScatteringPrecomputedPrograms_;
    std::vector<ShaderProgPtr> eclipsedDoubleScatteringPrecomputationPrograms_;
    // Indexed as eclipsedSingleScatteringPrecomputationPrograms_[scattererName][wavelengthSetIndex]
    std::unique_ptr<ScatteringProgramsMap> eclipsedSingleScatteringPrecomputationPrograms_;
    ShaderProgPtr luminanceToScreenRGB_;
    ShaderProgPtr viewDirectionGetterProgram_;
    std::map<ScattererName,bool> scatterersEnabledStates_;

    DragMode dragMode_=DragMode::None;
    int prevMouseX_, prevMouseY_;

    int numAltIntervalsIn4DTexture_;
    int numAltIntervalsInEclipsed4DTexture_;

    bool readyToRender_=false;

    DEFINE_EXPLICIT_BOOL(CountStepsOnly);
    void loadTextures(CountStepsOnly countStepsOnly);
    void reloadScatteringTextures(CountStepsOnly countStepsOnly);
    void setupRenderTarget();
    void loadShaders(CountStepsOnly countStepsOnly);
    void setupBuffers();
    void clearResources();
    void tick(int loadingStepsDone);
    void reportLoadingFinished();

    double altitudeUnitRangeTexCoord() const;
    double moonAngularRadius() const;
    double cameraMoonDistance() const;
    glm::dvec3 sunDirection() const;
    glm::dvec3 moonPosition() const;
    glm::dvec3 moonPositionRelativeToSunAzimuth() const;
    glm::dvec3 cameraPosition() const;
    QVector3D rgbMaxValue() const;
    void makeBayerPatternTexture();
    glm::ivec2 loadTexture2D(QString const& path);
    void loadTexture4D(QString const& path, float altitudeCoord);
    void load4DTexAltitudeSlicePair(QString const& path, QOpenGLTexture& texLower, QOpenGLTexture& texUpper, float altitudeCoord);
    void updateAltitudeTexCoords(float altitudeCoord, double* floorAltIndex = nullptr);
    void updateEclipsedAltitudeTexCoords(float altitudeCoord, double* floorAltIndex = nullptr);

    void precomputeEclipsedSingleScattering();
    void precomputeEclipsedDoubleScattering();
    void renderZeroOrderScattering();
    void renderSingleScattering();
    void renderMultipleScattering();
    void clearRadianceFrames();
    bool canGrabRadiance() const;

signals:
    void needRedraw();
};

#endif
