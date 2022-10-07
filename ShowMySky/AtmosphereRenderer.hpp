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
#include "api/ShowMySky/AtmosphereRenderer.hpp"

class AtmosphereRenderer : public ShowMySky::AtmosphereRenderer
{
    using ShaderProgPtr=std::unique_ptr<QOpenGLShaderProgram>;
    using TexturePtr=std::unique_ptr<QOpenGLTexture>;
    using ScattererName=QString;
    QOpenGLFunctions_3_3_Core& gl;
public:

    /**
     * This is the constructor of the renderer. It saves the reference to \p gl, \p tools for later use (so these objects must outlive AtmosphereRenderer), also saves the \p drawSurface callback. Then the model description file `params.atmo` inside the directory pointed to \p pathToData is parsed.
     *
     * If an error occurs while parsing the model description file, ShowMySky::Error is thrown.
     *
     * The callback function \p drawSurface has one parameter: a shader program \p shprog that includes, aside from the core atmosphere model GLSL code, the shaders \p viewDirVertShaderSrc and \p viewDirFragShaderSrc that have been passed to #initDataLoading method (this should be done by the application before any other calls). By the point when this callback is called, \p shprog has already been bound to current OpenGL context (via \c glUseProgram).
     *
     * The purpose of \p shprog is to enable setting of the necessary uniform values via the \c glUniform* family of functions to make the drawing work. It also includes the \c calcViewDir function implemented in \p viewDirVertShaderSrc shader, so the necessary uniform values (if any) for the operation of \c calcViewDir should also to be set in this callback before issuing any draw calls.
     *
     * To create an instance of ::AtmosphereRenderer indirectly having `dlopen`ed the `ShowMySky` library, use ::ShowMySky_AtmosphereRenderer_create.
     *
     * \param gl QtOpenGL-provided OpenGL 3.3 function resolver;
     * \param pathToData path to the data directory of the atmosphere model that contains `params.atmo`;
     * \param tools pointer to an implementation of the ShowMySky::Settings interface;
     * \param drawSurface a callback function that will be called each time a surface is to be rendered.
     */
    AtmosphereRenderer(QOpenGLFunctions_3_3_Core& gl,
                       QString const& pathToData,
                       ShowMySky::Settings* tools,
                       std::function<void(QOpenGLShaderProgram& shprog)> const& drawSurface);
    AtmosphereRenderer(AtmosphereRenderer const&)=delete;
    AtmosphereRenderer(AtmosphereRenderer&&)=delete;
    ~AtmosphereRenderer();
    void setDrawSurfaceCallback(std::function<void(QOpenGLShaderProgram& shprog)> const& drawSurface) override;
    int initDataLoading(QByteArray viewDirVertShaderSrc, QByteArray viewDirFragShaderSrc,
                        std::vector<std::pair<std::string,GLuint>> viewDirBindAttribLocations) override;
    LoadingStatus stepDataLoading() override;
    int initPreparationToDraw() override;
    LoadingStatus stepPreparationToDraw() override;
    QString currentActivity() const override { return currentActivity_; }
    bool isLoading() const override { return totalLoadingStepsToDo_ > 0; }
    bool isReadyToRender() const override { return state_ == State::ReadyToRender; }
    bool canGrabRadiance() const override;
    bool canSetSolarSpectrum() const override;
    bool canRenderPrecomputedEclipsedDoubleScattering() const override;
    GLuint getLuminanceTexture() override { return luminanceRenderTargetTexture_.textureId(); };

    void draw(double brightness, bool clear) override;
    void resizeEvent(int width, int height) override;
    QVector4D getPixelLuminance(QPoint const& pixelPos) override;
    SpectralRadiance getPixelSpectralRadiance(QPoint const& pixelPos) override;
    std::vector<float> getWavelengths() override;
    void setSolarSpectrum(std::vector<float> const& solarIrradianceAtTOA) override;
    void resetSolarSpectrum() override;
    Direction getViewDirection(QPoint const& pixelPos) override;

    void setScattererEnabled(QString const& name, bool enable) override;
    int initShaderReloading() override;
    LoadingStatus stepShaderReloading() override;
    AtmosphereParameters const& atmosphereParameters() const { return params_; }

private: // variables
    ShowMySky::Settings* tools_;
    std::function<void(QOpenGLShaderProgram&)> drawSurfaceCallback;
    AtmosphereParameters params_;
    QString pathToData_;
    int totalLoadingStepsToDo_=-1, loadingStepsDone_=0, currentLoadingIterationStepCounter_=0;
    QString currentActivity_;

    QByteArray viewDirVertShaderSrc_, viewDirFragShaderSrc_;
    std::vector<std::pair<std::string,GLuint>> viewDirBindAttribLocations_;

    GLuint vao_=0, vbo_=0, luminanceRadianceFBO_=0, viewDirectionFBO_=0;
    GLuint eclipseSingleScatteringPrecomputationFBO_=0;
    GLuint eclipseDoubleScatteringPrecomputationFBO_=0;
    // Lower and upper altitude slices from the 4D texture
    std::vector<TexturePtr> eclipsedDoubleScatteringTextures_;
    std::vector<TexturePtr> multipleScatteringTextures_;
    std::vector<TexturePtr> transmittanceTextures_;
    std::vector<TexturePtr> irradianceTextures_;
    std::vector<TexturePtr> lightPollutionTextures_;
    std::vector<GLuint> radianceRenderBuffers_;
    std::map<ScattererName,std::vector<TexturePtr>> singleScatteringInterpolationGuidesTextures01_; // VZA-dotViewSun dimensions
    std::map<ScattererName,std::vector<TexturePtr>> singleScatteringInterpolationGuidesTextures02_; // VZA-SZA dimensions
    GLuint viewDirectionRenderBuffer_=0;
    // Indexed as singleScatteringTextures_[scattererName][wavelengthSetIndex]
    std::map<ScattererName,std::vector<TexturePtr>> singleScatteringTextures_;
    std::map<ScattererName,std::vector<TexturePtr>> eclipsedSingleScatteringPrecomputationTextures_;
    TexturePtr eclipsedDoubleScatteringPrecomputationScratchTexture_;
    std::vector<TexturePtr> eclipsedDoubleScatteringPrecomputationTargetTextures_;
    QOpenGLTexture luminanceRenderTargetTexture_;
    QSize viewportSize_;
    double altCoordToLoad_=0; //!< Used to load textures for a single altitude slice, even if input altitude changes during the load

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
    std::unique_ptr<QOpenGLShader> precomputationProgramsVertShader_;
    std::unique_ptr<QOpenGLShader> viewDirVertShader_, viewDirFragShader_;
    ShaderProgPtr viewDirectionGetterProgram_;
    std::map<ScattererName,bool> scatterersEnabledStates_;

    std::vector<QVector4D> solarIrradianceFixup_;

    int numAltIntervalsIn4DTexture_;

    enum class State
    {
        NotReady,           //!< Just constructed or failed to load data
        LoadingData,        //!< After initDataLoading() and until loading completes
        ReloadingShaders,   //!< After initShaderReloading() and until shaders reloading completes
        ReloadingTextures,  //!< After initPreparationToDraw() and until textures reloading completes
        ReadyToRender,
    } state_ = State::NotReady;

private: // methods
    DEFINE_EXPLICIT_BOOL(CountStepsOnly);
    void loadTextures(CountStepsOnly countStepsOnly);
    void reloadScatteringTextures(CountStepsOnly countStepsOnly);
    void setupRenderTarget();
    void loadShaders(CountStepsOnly countStepsOnly);
    void setupBuffers();
    void clearResources();
    void finalizeLoading();
    void drawSurface(QOpenGLShaderProgram& prog);

    double altitudeUnitRangeTexCoord() const;
    double cameraMoonDistance() const;
    glm::dvec3 sunDirection() const;
    glm::dvec3 moonPosition() const;
    glm::dvec3 moonPositionRelativeToSunAzimuth() const;
    glm::dvec3 cameraPosition() const;
    glm::ivec2 loadTexture2D(QString const& path);
    enum class Texture4DType
    {
        ScatteringTexture,
        InterpolationGuides,
    };
    void loadTexture4D(QString const& path, float altitudeCoord, Texture4DType texType = Texture4DType::ScatteringTexture);
    void loadEclipsedDoubleScatteringTexture(QString const& path, float altitudeCoord);

    void precomputeEclipsedSingleScattering();
    void precomputeEclipsedDoubleScattering();
    void renderZeroOrderScattering();
    void renderSingleScattering();
    void renderMultipleScattering();
    void renderLightPollution();
    void prepareRadianceFrames(bool clear);
};

#endif
