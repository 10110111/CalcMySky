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

class ToolsWidget;
class AtmosphereRenderer : public QObject
{
    Q_OBJECT

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
    };

    AtmosphereRenderer(QOpenGLFunctions_3_3_Core& gl, QString const& pathToData, Parameters const& params, ToolsWidget* tools);
    AtmosphereRenderer(AtmosphereRenderer const&)=delete;
    AtmosphereRenderer(AtmosphereRenderer&&)=delete;
    ~AtmosphereRenderer();

    void draw();
    void setDragMode(DragMode mode, int x=0, int y=0) { dragMode=mode; prevMouseX=x; prevMouseY=y; }
    void mouseMove(int x, int y);
    void resizeEvent(int width, int height);
    void setScattererEnabled(int scattererIndex, bool enable);
    void reloadShaders(QString const& pathToData);
    QVector<QString> const& scattererNames() const { return scattererNames_; }

private:
    enum SingleScatteringRenderMode
    {
        SSRM_ON_THE_FLY,
        SSRM_PRECOMPUTED,

        SSRM_COUNT
    };
    static constexpr const char* singleScatteringRenderModeNames[SSRM_COUNT]={"on-the-fly", "precomputed"};

    ToolsWidget* tools;
    unsigned wavelengthSetCount;

    GLuint vao=0, vbo=0, fbo=0;
    std::vector<std::unique_ptr<QOpenGLTexture>> transmittanceTextures;
    std::vector<std::unique_ptr<QOpenGLTexture>> irradianceTextures;
    // Indexed as singleScatteringTextures[wavelengthSetIndex][scattererIndex]
    std::vector<std::vector<std::unique_ptr<QOpenGLTexture>>> singleScatteringTextures;
    QOpenGLTexture multipleScatteringTexture;
    QOpenGLTexture bayerPatternTexture;
    QOpenGLTexture texFBO;

    std::vector<std::unique_ptr<QOpenGLShaderProgram>> zeroOrderScatteringPrograms;
    // Indexed as singleScatteringPrograms[renderMode][wavelengthSetIndex][scattererIndex]
    std::vector<std::vector<std::vector<std::unique_ptr<QOpenGLShaderProgram>>>> singleScatteringPrograms;
    std::unique_ptr<QOpenGLShaderProgram> multipleScatteringProgram;
    std::unique_ptr<QOpenGLShaderProgram> luminanceToScreenRGB;
    // Indexed as scatterersEnabledStates[scattererIndex]
    QVector<bool> scatterersEnabledStates;
    QVector<QString> scattererNames_;

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
