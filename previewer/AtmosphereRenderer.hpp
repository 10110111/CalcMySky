#ifndef INCLUDE_ONCE_98A84326_8B1E_4A9E_98E9_9E2D7305E730
#define INCLUDE_ONCE_98A84326_8B1E_4A9E_98E9_9E2D7305E730

#include <cmath>
#include <QObject>
#include <QOpenGLFunctions_3_3_Core>

class AtmosphereRenderer : public QObject
{
    Q_OBJECT

    QOpenGLFunctions_3_3_Core& gl;
public:
    using Vec4=std::array<GLfloat,4>;
    enum class DragMode
    {
        None,
        Sun,
        Camera,
    };

    AtmosphereRenderer(QOpenGLFunctions_3_3_Core& gl, std::string const& pathToData);
    AtmosphereRenderer(AtmosphereRenderer const&)=delete;
    AtmosphereRenderer(AtmosphereRenderer&&)=delete;
    ~AtmosphereRenderer();

    void draw();
    void setDragMode(DragMode mode, int x=0, int y=0) { dragMode=mode; prevMouseX=x; prevMouseY=y; }
    void mouseMove(int x, int y);
    void setAltitude(double altitude);
    void setExposure(double exposure);
    void setFovY(double fovY);
    void resizeEvent(int width, int height);
    double sunElevation() const;
    void setSunElevation(float elevation);
    double sunAzimuth() const;
    void setSunAzimuth(float azimuth);
    Vec4 meanPixelValue() const { return meanPixelValue_; }

private:
    GLuint vao, vbo;
    enum
    {
        TRANSMITTANCE_TEXTURE,
        SCATTERING_TEXTURE,
        IRRADIANCE_TEXTURE,
        MIE_SCATTERING_TEXTURE,

        TEX_FBO,

        TEX_COUNT
    };
    GLuint textures[TEX_COUNT];
    GLuint program;
    GLuint fbo;

    double viewZenithAngle_=M_PI/2, viewAzimuth_=0;
    double sunZenithAngle_=M_PI/4, sunAzimuth_=M_PI/7;
    double altitudeInMeters=50;
    double exposure=1e-4;
    double fovY=50*M_PI/180;
    DragMode dragMode=DragMode::None;
    int prevMouseX, prevMouseY;
    Vec4 meanPixelValue_;

    void loadTextures(std::string const& pathToData);
    void setupRenderTarget();
    void loadShaders(std::string const& pathToData);
    void setupBuffers();
    Vec4 getMeanPixelValue(int texW, int texH);
    void bindAndSetupTexture(GLenum target, GLuint texture);
    GLuint makeShader(GLenum type, std::string const& srcFilePath);

signals:
    void sunElevationChanged(double elevation);
    void sunAzimuthChanged(double azimuth);
};

#endif
