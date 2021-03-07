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
#include "api/AtmosphereRenderer.hpp"

class AtmosphereRenderer : public QObject, public ShowMySky::AtmosphereRenderer
{
    Q_OBJECT

    using ShaderProgPtr=std::unique_ptr<QOpenGLShaderProgram>;
    using TexturePtr=std::unique_ptr<QOpenGLTexture>;
    using ScattererName=QString;
    QOpenGLFunctions_3_3_Core& gl;
public:

    AtmosphereRenderer(QOpenGLFunctions_3_3_Core& gl,
                       QString const& pathToData,
                       ShowMySky::Settings* tools,
                       std::function<void(QOpenGLShaderProgram&)> drawSurface);
    AtmosphereRenderer(AtmosphereRenderer const&)=delete;
    AtmosphereRenderer(AtmosphereRenderer&&)=delete;
    ~AtmosphereRenderer();
    void loadData(QByteArray viewDirVertShaderSrc, QByteArray viewDirFragShaderSrc) override;
    bool readyToRender() const override { return readyToRender_; }
    bool canGrabRadiance() const override;
    GLuint getLuminanceTexture() override { return luminanceRenderTargetTexture_.textureId(); };

    void draw(double brightness, bool clear) override;
    void resizeEvent(int width, int height) override;
    SpectralRadiance getPixelSpectralRadiance(QPoint const& pixelPos) override;
    Direction getViewDirection(QPoint const& pixelPos) override;
    QObject* asQObject() override { return this; }

    void setScattererEnabled(QString const& name, bool enable) override;
    void reloadShaders() override;
    AtmosphereParameters const& atmosphereParameters() const { return params_; }

signals:
    void loadProgress(QString const& currentActivity, int stepsDone, int stepsToDo) override;

private: // variables
    ShowMySky::Settings* tools_;
    std::function<void(QOpenGLShaderProgram&)> drawSurfaceCallback;
    AtmosphereParameters params_;
    QString pathToData_;
    int totalLoadingStepsToDo_=-1, loadingStepsDone_=0;
    QString currentActivity_;

    QByteArray viewDirVertShaderSrc_, viewDirFragShaderSrc_;

    GLuint vao_=0, vbo_=0, luminanceRadianceFBO_=0, viewDirectionFBO_=0;
    GLuint eclipseSingleScatteringPrecomputationFBO_=0;
    GLuint eclipseDoubleScatteringPrecomputationFBO_=0;
    // Lower and upper altitude slices from the 4D texture
    std::vector<TexturePtr> eclipsedDoubleScatteringTexturesLower_, eclipsedDoubleScatteringTexturesUpper_;
    std::vector<TexturePtr> multipleScatteringTextures_;
    std::vector<TexturePtr> transmittanceTextures_;
    std::vector<TexturePtr> irradianceTextures_;
    std::vector<TexturePtr> lightPollutionTextures_;
    std::vector<GLuint> radianceRenderBuffers_;
    GLuint viewDirectionRenderBuffer_=0;
    // Indexed as singleScatteringTextures_[scattererName][wavelengthSetIndex]
    std::map<ScattererName,std::vector<TexturePtr>> singleScatteringTextures_;
    std::map<ScattererName,std::vector<TexturePtr>> eclipsedSingleScatteringPrecomputationTextures_;
    TexturePtr eclipsedDoubleScatteringPrecomputationScratchTexture_;
    std::vector<TexturePtr> eclipsedDoubleScatteringPrecomputationTargetTextures_;
    QOpenGLTexture luminanceRenderTargetTexture_;
    QSize viewportSize_;
    float loadedAltitudeURTexCoordRange_[2]={NAN,NAN};
    float loadedEclipsedDoubleScatteringAltitudeURTexCoordRange_[2]={NAN,NAN};
    float staticAltitudeTexCoord_=-1;
    float eclipsedDoubleScatteringAltitudeAlphaUpper_=-1;

    std::vector<ShaderProgPtr> lightPollutionPrograms_;
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
    ShaderProgPtr viewDirectionGetterProgram_;
    std::map<ScattererName,bool> scatterersEnabledStates_;

    int numAltIntervalsIn4DTexture_;
    int numAltIntervalsInEclipsed4DTexture_;

    bool readyToRender_=false;

private: // methods
    DEFINE_EXPLICIT_BOOL(CountStepsOnly);
    void loadTextures(CountStepsOnly countStepsOnly);
    void reloadScatteringTextures(CountStepsOnly countStepsOnly);
    void setupRenderTarget();
    void loadShaders(CountStepsOnly countStepsOnly);
    void setupBuffers();
    void clearResources();
    void tick(int loadingStepsDone);
    void reportLoadingFinished();
    void drawSurface(QOpenGLShaderProgram& prog);

    double altitudeUnitRangeTexCoord() const;
    double moonAngularRadius() const;
    double cameraMoonDistance() const;
    glm::dvec3 sunDirection() const;
    glm::dvec3 moonPosition() const;
    glm::dvec3 moonPositionRelativeToSunAzimuth() const;
    glm::dvec3 cameraPosition() const;
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
    void renderLightPollution();
    void prepareRadianceFrames(bool clear);
};

#endif
