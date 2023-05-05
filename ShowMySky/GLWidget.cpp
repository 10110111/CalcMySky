#include "GLWidget.hpp"
#include <chrono>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QFileDialog>
#include <QMessageBox>
#include <QSurfaceFormat>
#include <glm/gtx/transform.hpp>
#include "../common/util.hpp"
#include "../common/const.hpp"
#include "util.hpp"
#include "ToolsWidget.hpp"
#include "AtmosphereRenderer.hpp"
#include "GLSLCosineQualityChecker.hpp"
#include "BlueNoiseTriangleRemapped.hpp"

static QPoint position(QMouseEvent* event)
{
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    return event->pos();
#else
    return event->position().toPoint();
#endif
}

GLWidget::GLWidget(QString const& pathToData, ToolsWidget* tools, QWidget* parent)
    : QOpenGLWidget(parent)
    , ditherPatternTexture_(QOpenGLTexture::Target2D)
    , pathToData(pathToData)
    , tools(tools)
{
    installEventFilter(this);
    setFocusPolicy(Qt::StrongFocus);
    setUpdateBehavior(QOpenGLWidget::PartialUpdate);
}

GLWidget::~GLWidget()
{
    // Let the destructor of renderer have current GL context. This avoids warnings from QOpenGLTexturePrivate::destroy().
    // We also want to do our own cleanup.
    makeCurrent();

    if(vbo_)
    {
        glDeleteBuffers(1, &vbo_);
        vbo_=0;
    }
    if(vao_)
    {
        glDeleteVertexArrays(1, &vao_);
        vao_=0;
    }
    if(glareTextures_[0])
    {
        glDeleteTextures(std::size(glareTextures_), glareTextures_);
        std::fill_n(glareTextures_, std::size(glareTextures_), 0);
    }
    if(glareFBOs_[0])
    {
        glDeleteFramebuffers(std::size(glareFBOs_), glareFBOs_);
        std::fill_n(glareFBOs_, std::size(glareFBOs_), 0);
    }
}

void GLWidget::makeDitherPatternTexture()
{
    ditherPatternTexture_.setMinificationFilter(QOpenGLTexture::Nearest);
    ditherPatternTexture_.setMagnificationFilter(QOpenGLTexture::Nearest);
    ditherPatternTexture_.setWrapMode(QOpenGLTexture::Repeat);
    ditherPatternTexture_.bind();
    switch(tools->ditheringMethod())
    {
    case DitheringMethod::NoDithering:
    {
        static const float zero=0;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, 1,1, 0, GL_RED, GL_FLOAT, &zero);
        break;
    }
    case DitheringMethod::BlueNoiseTriangleRemapped:
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, std::size(blueNoiseTriangleRemapped), std::size(blueNoiseTriangleRemapped[0]),
                     0, GL_RED, GL_FLOAT, blueNoiseTriangleRemapped);
        break;
    case DitheringMethod::Bayer:
    {
        static constexpr int width=8, height=8;
        static constexpr float bayerPattern[width*height] =
        {
            // 8x8 Bayer ordered dithering pattern.
            0/64.f, 32/64.f,  8/64.f, 40/64.f,  2/64.f, 34/64.f, 10/64.f, 42/64.f,
            48/64.f, 16/64.f, 56/64.f, 24/64.f, 50/64.f, 18/64.f, 58/64.f, 26/64.f,
            12/64.f, 44/64.f,  4/64.f, 36/64.f, 14/64.f, 46/64.f,  6/64.f, 38/64.f,
            60/64.f, 28/64.f, 52/64.f, 20/64.f, 62/64.f, 30/64.f, 54/64.f, 22/64.f,
            3/64.f, 35/64.f, 11/64.f, 43/64.f,  1/64.f, 33/64.f,  9/64.f, 41/64.f,
            51/64.f, 19/64.f, 59/64.f, 27/64.f, 49/64.f, 17/64.f, 57/64.f, 25/64.f,
            15/64.f, 47/64.f,  7/64.f, 39/64.f, 13/64.f, 45/64.f,  5/64.f, 37/64.f,
            63/64.f, 31/64.f, 55/64.f, 23/64.f, 61/64.f, 29/64.f, 53/64.f, 21/64.f
        };
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, width, height, 0, GL_RED, GL_FLOAT, bayerPattern);
        break;
    }
    default:
        std::abort();
    }
}

void GLWidget::makeGlareRenderTarget()
{
    if(!glareTextures_[0])
        glGenTextures(std::size(glareTextures_), glareTextures_);
    for(unsigned n=0; n<std::size(glareTextures_); ++n)
    {
        glBindTexture(GL_TEXTURE_2D, glareTextures_[n]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width(), height(), 0, GL_RGBA, GL_FLOAT, nullptr);
        // This is needed to avoid aliasing when sampling along skewed lines
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        // We want our convolution filter to sample zeros outside the texture, so clamp to _border_
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    }
    if(!glareFBOs_[0])
        glGenFramebuffers(std::size(glareFBOs_), glareFBOs_);
    for(unsigned n=0; n<std::size(glareFBOs_); ++n)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, glareFBOs_[n]);
        glFramebufferTexture(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,glareTextures_[n],0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}

QVector3D GLWidget::rgbMaxValue() const
{
    switch(tools->ditheringMode())
	{
		default:
		case DitheringMode::Color666:
			return QVector3D(63,63,63);
		case DitheringMode::Color565:
			return QVector3D(31,63,31);
		case DitheringMode::Color888:
			return QVector3D(255,255,255);
		case DitheringMode::Color101010:
			return QVector3D(1023,1023,1023);
	}
}

void GLWidget::initializeGL()
{
    if(!initializeOpenGLFunctions())
    {
        throw InitializationError{tr("Failed to initialize OpenGL %1.%2 functions")
                                    .arg(QSurfaceFormat::defaultFormat().majorVersion())
                                    .arg(QSurfaceFormat::defaultFormat().minorVersion())};
    }

    try
    {
        if(!ShowMySky_AtmosphereRenderer_create)
        {
            QLibrary showMySky(SHOWMYSKY_LIB_NAME, ShowMySky_ABI_version);
            if(!showMySky.load())
                throw DataLoadError(tr("Failed to load ShowMySky library: %1").arg(showMySky.errorString()));
            const auto abi=reinterpret_cast<const quint32*>(showMySky.resolve("ShowMySky_ABI_version"));
            if(!abi)
                throw DataLoadError(tr("Failed to determine ABI version of ShowMySky library."));
            if(*abi != ShowMySky_ABI_version)
                throw DataLoadError(tr("ABI version of ShowMySky library is %1, but this program has been compiled against version %2.")
                                    .arg(*abi).arg(ShowMySky_ABI_version));
            ShowMySky_AtmosphereRenderer_create=reinterpret_cast<decltype(ShowMySky_AtmosphereRenderer_create)>(
                                                showMySky.resolve("ShowMySky_AtmosphereRenderer_create"));
            if(!ShowMySky_AtmosphereRenderer_create)
                throw DataLoadError(tr("Failed to resolve the function to create AtmosphereRenderer"));
        }

        const std::function drawSurface=[this](QOpenGLShaderProgram& program)
        {
            program.setUniformValue("zoomFactor", tools->zoomFactor());
            {
                const float yaw=tools->cameraYaw();
                const float pitch=tools->cameraPitch();
                const auto camYaw=glm::rotate(yaw, glm::vec3(0,0,1));
                const auto camPitch=glm::rotate(pitch, glm::vec3(0,-1,0));
                program.setUniformValue("cameraRotation", toQMatrix(camYaw*camPitch));
                program.setUniformValue("viewportAspectRatio", float(width())/float(height()));
                program.setUniformValue("projection", static_cast<int>(currentProjection()));
            }
            glBindVertexArray(vao_);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            glBindVertexArray(0);
        };
        renderer.reset(ShowMySky_AtmosphereRenderer_create(this,&pathToData,tools,&drawSurface));
        tools->updateParameters(static_cast<AtmosphereRenderer*>(renderer.get())->atmosphereParameters());
        connect(tools, &ToolsWidget::settingChanged, this, qOverload<>(&GLWidget::update));
        connect(tools, &ToolsWidget::projectionChanged, this, [this](const Projection newProjection)
                { currentProjection_ = newProjection; update(); });
        connect(tools, &ToolsWidget::colorModeChanged, this, [this](const ColorMode newColorMode)
                { currentColorMode_ = newColorMode; update(); });
        connect(tools, &ToolsWidget::ditheringMethodChanged, this, [this]
                { makeDitherPatternTexture(); update(); });
        connect(tools, &ToolsWidget::setScattererEnabled, this, [this,renderer=renderer.get()](QString const& name, const bool enable)
                { renderer->setScattererEnabled(name, enable); update(); });
        connect(tools, &ToolsWidget::reloadShadersClicked, this, &GLWidget::reloadShaders);
        connect(tools, &ToolsWidget::resetSolarSpectrum, this, &GLWidget::resetSolarSpectrum);
        connect(tools, &ToolsWidget::setFlatSolarSpectrum, this, &GLWidget::setFlatSolarSpectrum);
        connect(tools, &ToolsWidget::setBlackBodySolarSpectrum, this, &GLWidget::setBlackBodySolarSpectrum);

        makeDitherPatternTexture();
        makeGlareRenderTarget();
        setupBuffers();

        GLSLCosineQualityChecker cosineChecker(*this);
        const bool cosineIsOK = cosineChecker.isGood();

        luminanceToScreenRGB_=std::make_unique<QOpenGLShaderProgram>();
        addShaderCode(*luminanceToScreenRGB_, QOpenGLShader::Fragment, tr("luminanceToScreenRGB fragment shader"), (1+R"(
#version 330
uniform float exposure;
uniform sampler2D luminanceXYZW;

uniform int colorMode;
#define COLMOD_SRGB 0
#define COLMOD_SCOTOPIC_LUMINANCE 1
#define COLMOD_PHOTOPIC_LUMINANCE 2
#define COLMOD_XYZ_CHROMATICITY 3
#define COLMOD_SRGBL_CHROMATICITY 4
#define COLMOD_SRGBL_CHROMATICITY_TO_MAX 5
#define COLMOD_HUE 6

in vec2 texCoord;
out vec4 color;

#define DM_NONE )"+std::to_string(static_cast<int>(DitheringMethod::NoDithering))+R"(
#define DM_BAYER )"+std::to_string(static_cast<int>(DitheringMethod::Bayer))+R"(
#define DM_BLUE_TRIANG )"+std::to_string(static_cast<int>(DitheringMethod::BlueNoiseTriangleRemapped))+R"(
uniform int ditheringMethod;
uniform bool gradualClipping;
uniform vec3 rgbMaxValue;
uniform sampler2D ditherPattern;
vec3 dither_BlueTriang(vec3 c)
{
    vec3 noise=texture(ditherPattern,gl_FragCoord.xy/64.).rrr;

    {
        // Prevent undershoot (imperfect white) due to clipping of positive noise contributions
        vec3 antiUndershootC = 1+(0.5-sqrt(2*rgbMaxValue*(1-c)))/rgbMaxValue;
        vec3 edge = 1-1/(2*rgbMaxValue);
        // Per-component version of: c = c > edge ? antiUndershootC : c;
        c = antiUndershootC + step(-edge, -c) * (c-antiUndershootC);
    }

    {
        // Prevent overshoot (imperfect black) due to clipping of negative noise contributions
        vec3 antiOvershootC  = (-1+sqrt(8*rgbMaxValue*c))/(2*rgbMaxValue);
        vec3 edge = 1/(2*rgbMaxValue);
        // Per-component version of: c = c < edge ? antiOvershootC : c;
        c = antiOvershootC + step(edge, c) * (c-antiOvershootC);
    }

    return c+noise/rgbMaxValue;
}

vec3 dither_Bayer(vec3 c)
{
    vec3 bayer=texture(ditherPattern,gl_FragCoord.xy/8.).rrr;

    vec3 rgb=c*rgbMaxValue;
    vec3 head=floor(rgb);
    vec3 tail=rgb-head;
    return (head+1.-step(tail,bayer))/rgbMaxValue;
}


vec3 clip(vec3 rgb)
{
    rgb=max(vec3(0), rgb);
    return sqrt(tanh(rgb*rgb));
}

vec3 sRGBTransferFunction(const vec3 c)
{
    vec3 s = step(vec3(0.0031308), c);
    vec3 d = vec3(1) - s;
    return s * (1.055*pow(c, vec3(1./2.4))-0.055) +
               d *  12.92*c;
}

vec3 applyClipping(const vec3 c)
{
    return gradualClipping ? clip(c) : clamp(c, 0., 1.);
}

const mat3 XYZ2sRGBl=mat3(vec3(3.2406,-0.9689,0.0557),
                          vec3(-1.5372,1.8758,-0.204),
                          vec3(-0.4986,0.0415,1.057));
vec3 XYZ2sRGB(const vec3 XYZ)
{
    vec3 rgb=XYZ2sRGBl*XYZ;
    vec3 clippedRGB = applyClipping(rgb);
    return sRGBTransferFunction(clippedRGB);
}

vec3 markOutOfRangeValues(const vec3 c)
{
    if(max(c.r, max(c.g, c.b)) > 1)
        return vec3(mod(gl_FragCoord.x+gl_FragCoord.y, 5) > 2.5 ? 1 : 0);
    if(max(c.r, max(c.g, c.b)) < 0)
        return mod(gl_FragCoord.x+gl_FragCoord.y, 5) > 2.5 ? vec3(1,0,1) : vec3(0);
    return c;
}

void main()
{
    vec4 tex=texture(luminanceXYZW, texCoord);

    vec3 smoothColorOutput;
    if(colorMode == COLMOD_SRGB)
        smoothColorOutput = XYZ2sRGB(exposure * tex.xyz);
    else if(colorMode == COLMOD_SCOTOPIC_LUMINANCE)
        smoothColorOutput = applyClipping(vec3(exposure * tex.w));
    else if(colorMode == COLMOD_PHOTOPIC_LUMINANCE)
        smoothColorOutput = applyClipping(vec3(exposure * tex.y));
    else if(colorMode == COLMOD_XYZ_CHROMATICITY)
        smoothColorOutput = XYZ2sRGB(markOutOfRangeValues(exposure * tex.xyz / tex.y));
    else if(colorMode == COLMOD_SRGBL_CHROMATICITY)
    {
        vec3 rgb=XYZ2sRGBl*tex.xyz;
        float normCoef = (rgb.r + rgb.g + rgb.b);
        smoothColorOutput = sRGBTransferFunction(markOutOfRangeValues(exposure * rgb / normCoef));
    }
    else if(colorMode == COLMOD_SRGBL_CHROMATICITY_TO_MAX)
    {
        vec3 rgb=XYZ2sRGBl*tex.xyz;
        float maxValue = max(rgb.r, max(rgb.g, rgb.b));
        smoothColorOutput = sRGBTransferFunction(rgb / maxValue);
    }
    else if(colorMode == COLMOD_HUE)
    {
        vec3 rgb=XYZ2sRGBl*tex.xyz;
        float minValue = min(rgb.r, min(rgb.g, rgb.b));
        vec3 saturated = rgb - minValue;
        float maxValue = max(saturated.r, max(saturated.g, saturated.b));
        smoothColorOutput = sRGBTransferFunction(saturated / maxValue);
    }
    else
        smoothColorOutput = vec3(1,0,1);

    if(ditheringMethod==DM_BAYER)
        color=vec4(dither_Bayer(smoothColorOutput),1);
    else if(ditheringMethod==DM_BLUE_TRIANG)
        color=vec4(dither_BlueTriang(smoothColorOutput),1);
    else if(ditheringMethod==DM_NONE)
        color=vec4(smoothColorOutput,1);
}
)").c_str());
        addShaderCode(*luminanceToScreenRGB_, QOpenGLShader::Vertex, tr("luminanceToScreenRGB vertex shader"), 1+R"(
#version 330
in vec3 vertex;
out vec2 texCoord;
void main()
{
    texCoord=(vertex.xy+vec2(1))/2;
    gl_Position=vec4(vertex,1);
}
)");
        link(*luminanceToScreenRGB_, tr("luminanceToScreenRGB shader program"));

        glareProgram_=std::make_unique<QOpenGLShaderProgram>();
        addShaderCode(*glareProgram_, QOpenGLShader::Fragment, tr("glare fragment shader"), 1+R"(
#version 330
uniform sampler2D luminanceXYZW;
uniform vec2 stepDir;
out vec4 XYZW;

float weight(const float x)
{
    const float a=0.955491103831962;
    const float b=0.0111272240420095;
    return abs(x)<0.5 ? a : b/(x*x);
}

void main()
{
    vec2 size = textureSize(luminanceXYZW, 0);
    vec2 pos = gl_FragCoord.st-vec2(0.5);
    if(stepDir.x*stepDir.y >= 0)
    {
        vec2 dir = stepDir.x<0 || stepDir.y<0 ? -stepDir : stepDir;
        float stepCountBottomLeft = 1+ceil(min(pos.x/dir.x, pos.y/dir.y));
        float stepCountTopRight = 1+ceil(min((size.x-pos.x-1)/dir.x, (size.y-pos.y-1)/dir.y));

        XYZW = weight(0) * texture(luminanceXYZW, gl_FragCoord.st/size);
        for(float dist=1; dist<stepCountBottomLeft; ++dist)
            XYZW += weight(dist) * texture(luminanceXYZW, (gl_FragCoord.st-dir*dist)/size);
        for(float dist=1; dist<stepCountTopRight; ++dist)
            XYZW += weight(dist) * texture(luminanceXYZW, (gl_FragCoord.st+dir*dist)/size);
    }
    else
    {
        vec2 dir = stepDir.x<0 ? -stepDir : stepDir;
        float stepCountTopLeft = 1+ceil(min(pos.x/dir.x, (size.y-pos.y-1)/-dir.y));
        float stepCountBottomRight = 1+ceil(min((size.x-pos.x-1)/dir.x, pos.y/-dir.y));

        XYZW = weight(0) * texture(luminanceXYZW, gl_FragCoord.st/size);
        for(float dist=1; dist<stepCountTopLeft; ++dist)
            XYZW += weight(dist) * texture(luminanceXYZW, (gl_FragCoord.st-dir*dist)/size);
        for(float dist=1; dist<stepCountBottomRight; ++dist)
            XYZW += weight(dist) * texture(luminanceXYZW, (gl_FragCoord.st+dir*dist)/size);
    }
}
)");
        addShaderCode(*glareProgram_, QOpenGLShader::Vertex, tr("glare vertex shader"), 1+R"(
#version 330
in vec3 vertex;
void main()
{
    gl_Position=vec4(vertex,1);
}
)");
        link(*glareProgram_, tr("glare shader program"));

        static constexpr const char* viewDirVertShaderSrc=1+R"(
#version 330
in vec3 vertex;
out vec3 position;
void main()
{
    position=vertex;
    gl_Position=vec4(position,1);
}
)";
        QByteArray viewDirFragShaderSrc=1+R"(
#version 330
in vec3 position;
uniform float zoomFactor;
uniform mat3 cameraRotation;
uniform float viewportAspectRatio;

uniform int projection;
// These values must match the entries in the Projection enum
#define PROJ_EQUIRECTANGULAR 0
#define PROJ_PERSPECTIVE 1
#define PROJ_FISHEYE 2

const float PI=3.1415926535897932;

#if COSINE_IS_BROKEN
// Define Chebyshoff approximations for sin and cos
float sin(float x)
{
    x = mod(x+PI, 2*PI)-PI;
    return x*(0.999999599920672 + x*x*(-0.166665526354071 + x*x*(0.00833240298869917 + x*x*(-0.0001980863334175 + x*x*(2.69971463693744e-6 - 2.03622449118901e-8*x*x)))));
}
float cos(float x)
{
    x = mod(x+PI, 2*PI)-PI;
    return 0.999999210782322 + x*x*(-0.499994213384716 + x*x*(0.0416597778065509 + x*x*(-0.00138587899196014 + x*x*(0.0000242029413673591 - 2.19729638194131e-7*x*x))));
}
#endif

vec3 calcViewDir()
{
    vec2 pos=position.xy/zoomFactor;
    if(projection==PROJ_EQUIRECTANGULAR)
    {
        return cameraRotation*vec3(cos(pos.x*PI)*cos(pos.y*(PI/2)),
                                   sin(pos.x*PI)*cos(pos.y*(PI/2)),
                                   sin(pos.y*(PI/2)));
    }
    else if(projection==PROJ_PERSPECTIVE)
    {
        const float horizViewAngle = 120*PI/180;
        const float camDistToScreen = 0.5 * tan(horizViewAngle);
        pos.y /= viewportAspectRatio;
        return cameraRotation * normalize(vec3(-camDistToScreen, pos));
    }
    else if(projection==PROJ_FISHEYE)
    {
        const float thetaMax=PI;
        float r=length(pos.xy);
        float theta=r*thetaMax;
        if(theta > thetaMax)
            return vec3(0);
        float phi = PI - atan(pos.x,pos.y);
        return cameraRotation*vec3(cos(phi)*sin(theta),
                                   sin(phi)*sin(theta),
                                            cos(theta));
    }

    return vec3(0);
}
)";
        viewDirFragShaderSrc.replace("COSINE_IS_BROKEN", cosineIsOK ? "0" : "1");
        renderer->initDataLoading(viewDirVertShaderSrc, viewDirFragShaderSrc);
        stepDataLoading();
    }
    catch(ShowMySky::Error const& ex)
    {
        QTimer::singleShot(0,
            [this,errorType=ex.errorType(),what=ex.what()]
            {
                emit loadProgress(tr("Atmosphere renderer initialization failed"), 0, 0);
                QMessageBox::critical(this, errorType, what);
            });
    }
}

void GLWidget::stepDataLoading()
{
    try
    {
        makeCurrent();
        const auto status = renderer->stepDataLoading();
        emit loadProgress(renderer->currentActivity(), status.stepsDone, status.stepsToDo);
        if(renderer->isReadyToRender())
        {
            tools->setCanGrabRadiance(renderer->canGrabRadiance());
            tools->setCanSetSolarSpectrum(renderer->canSetSolarSpectrum());
            update();
        }
        else if(status.stepsDone < status.stepsToDo)
        {
            QTimer::singleShot(0, this, &GLWidget::stepDataLoading);
        }
    }
    catch(ShowMySky::Error const& ex)
    {
        QTimer::singleShot(0,
            [this,errorType=ex.errorType(),what=ex.what()]
            {
                emit loadProgress(tr("Data loading failed"), 0, 0);
                QMessageBox::critical(this, errorType, what);
            });
    }
}

void GLWidget::stepPreparationToDraw(const bool emitProgressStatus)
{
    try
    {
        makeCurrent();
        const auto status = renderer->stepPreparationToDraw();
        if(status.stepsToDo < 0) return; // This means we're not in time, the renderer has switched mode

        if(emitProgressStatus)
            emit loadProgress(renderer->currentActivity(), status.stepsDone, status.stepsToDo);

        if(status.stepsDone < status.stepsToDo)
            QTimer::singleShot(0, this, [this]{stepPreparationToDraw(true);});
        else
            QTimer::singleShot(0, this, qOverload<>(&GLWidget::update));
    }
    catch(ShowMySky::Error const& ex)
    {
        QTimer::singleShot(0,
            [this,errorType=ex.errorType(),what=ex.what()]
            {
                emit loadProgress(tr("Data loading failed"), 0, 0);
                QMessageBox::critical(this, errorType, what);
            });
    }
}

void GLWidget::paintGL()
{
    if(!renderer) return;
    if(!isVisible()) return;

    const int preparationSteps = renderer->initPreparationToDraw();
    if(preparationSteps > 0)
        stepPreparationToDraw(false);

    if(!renderer->isReadyToRender()) return;

    glFinish();
    const auto t0=std::chrono::steady_clock::now();
    renderer->draw(1, true);

    glBindVertexArray(vao_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, renderer->getLuminanceTexture());
    if(tools->glareEnabled())
    {
        // We want our convolution filter to sample zeros outside the texture, so clamp to _border_
        // Subsequent code doesn't depend on this
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        GLint targetFBO=-1;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &targetFBO);

        constexpr double degree=M_PI/180;
        constexpr double angleMin=5*degree;
        constexpr int numAngleSteps=3;
        constexpr double angleStep=360*degree/numAngleSteps;

        glareProgram_->bind();
        glareProgram_->setUniformValue("luminanceXYZW", 0);
        for(int angleStepNum=0; angleStepNum<numAngleSteps; ++angleStepNum)
        {
            // This is needed to avoid aliasing when sampling along skewed lines
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            const auto angle = angleMin + angleStep*angleStepNum;
            glareProgram_->setUniformValue("stepDir", QVector2D(std::cos(angle),std::sin(angle)));
            glBindFramebuffer(GL_FRAMEBUFFER, glareFBOs_[angleStepNum%2]);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            // Now use the result of this stage to feed the next stage
            glBindTexture(GL_TEXTURE_2D, glareTextures_[angleStepNum%2]);
        }

        glBindFramebuffer(GL_FRAMEBUFFER,targetFBO);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    luminanceToScreenRGB_->bind();
    luminanceToScreenRGB_->setUniformValue("luminanceXYZW", 0);
    ditherPatternTexture_.bind(1);
    luminanceToScreenRGB_->setUniformValue("ditherPattern", 1);
    luminanceToScreenRGB_->setUniformValue("rgbMaxValue", rgbMaxValue());
    luminanceToScreenRGB_->setUniformValue("ditheringMethod", static_cast<int>(tools->ditheringMethod()));
    luminanceToScreenRGB_->setUniformValue("gradualClipping", tools->gradualClippingEnabled());
    luminanceToScreenRGB_->setUniformValue("exposure", tools->exposure());
    luminanceToScreenRGB_->setUniformValue("colorMode", static_cast<int>(currentColorMode()));
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    glFinish();
    const auto t1=std::chrono::steady_clock::now();
    emit frameFinished(std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count());

    if(lastRadianceCapturePosition.x()>=0 && lastRadianceCapturePosition.y()>=0)
        updateSpectralRadiance(lastRadianceCapturePosition);
}

void GLWidget::resizeGL(int w, int h)
{
    if(!renderer) return;
    renderer->resizeEvent(w,h);
    makeGlareRenderTarget();
}

void GLWidget::updateSpectralRadiance(QPoint const& pixelPos)
{
    if(!renderer) return;
    makeCurrent();
    if(const auto spectrum=renderer->getPixelSpectralRadiance(pixelPos); !spectrum.empty())
    {
        if(tools->handleSpectralRadiance(spectrum))
            lastRadianceCapturePosition=pixelPos;
    }
}

void GLWidget::setFlatSolarSpectrum()
{
    const auto numWavelengths=renderer->getWavelengths().size();
    renderer->setSolarSpectrum(std::vector<float>(numWavelengths, 1.f));
    update();
}

void GLWidget::resetSolarSpectrum()
{
    renderer->resetSolarSpectrum();
    update();
}

static double blackBodySunSpectralIrradianceAtTOA(const double temperature, const double wavelength, const double earthSunDistance)
{
    using namespace std;
    return 1.814397573e38/pow(earthSunDistance,2)/pow(wavelength,5)/(exp(1.438777354e7/(temperature*wavelength))-1);
}

void GLWidget::setBlackBodySolarSpectrum(const double temperature)
{
    const auto wavelengths=renderer->getWavelengths();
    const auto& params = static_cast<AtmosphereRenderer*>(renderer.get())->atmosphereParameters();
    const auto earthSunDistance = params.earthSunDistance;
    std::vector<float> spectrum;
    for(const auto wavelength : wavelengths)
        spectrum.push_back(blackBodySunSpectralIrradianceAtTOA(temperature, wavelength, earthSunDistance));
    renderer->setSolarSpectrum(spectrum);
    update();
}

void GLWidget::wheelEvent(QWheelEvent* event)
{
    if(event->modifiers() & Qt::ControlModifier)
    {
        const auto stepSize = event->modifiers() & Qt::ShiftModifier ? 0.1 : 0.5;
        const auto increment = stepSize * event->angleDelta().y()/120.;
        tools->setZoomFactor(tools->zoomFactor() + increment);
    }
}

void GLWidget::mouseMoveEvent(QMouseEvent* event)
{
    if(event->buttons()==Qt::LeftButton && !(event->modifiers() & (Qt::ControlModifier|Qt::ShiftModifier)))
    {
        updateSpectralRadiance(event->pos());
        return;
    }

    const auto pos = position(event);
    switch(dragMode_)
    {
    case DragMode::Sun:
    {
        const auto mouseDeltaX = pos.x() - prevMouseX_;
        const auto mouseDeltaY = prevMouseY_ - pos.y();
        const auto oldZA=tools->sunZenithAngle(), oldAz=tools->sunAzimuth();
        switch(currentProjection())
        {
        case Projection::Equirectangular:
            tools->setSunZenithAngle(std::clamp(oldZA - mouseDeltaY*M_PI/height()/tools->zoomFactor(), 0., M_PI));
            tools->setSunAzimuth(std::remainder(oldAz + mouseDeltaX*2*M_PI/width()/tools->zoomFactor(), 2*M_PI));
            break;
        case Projection::Perspective:
        {
            const auto scaleX = 0.35;
            const auto scaleY = 0.75 * height() / width();
            tools->setSunZenithAngle(std::clamp(oldZA - scaleY*mouseDeltaY*M_PI/height()/tools->zoomFactor(), 0., M_PI));
            tools->setSunAzimuth(std::remainder(oldAz + scaleX*mouseDeltaX*2*M_PI/width()/tools->zoomFactor(), 2*M_PI));
            break;
        }
        case Projection::Fisheye:
        {
            const auto sunDir = glm::dvec3(std::cos(tools->sunAzimuth())*std::sin(tools->sunZenithAngle()),
                                           std::sin(tools->sunAzimuth())*std::sin(tools->sunZenithAngle()),
                                           std::cos(tools->sunZenithAngle()));
            const auto camYaw=glm::rotate(double(tools->cameraYaw()), glm::dvec3(0,0,1));
            const auto camPitch=glm::rotate(double(tools->cameraPitch()), glm::dvec3(0,-1,0));
            const auto cameraRotation = glm::dmat3(camYaw*camPitch);
            const auto invCamTimesSunDir = transpose(cameraRotation)*sunDir;
            const auto r = std::acos(std::clamp(invCamTimesSunDir.z, -1., 1.)) * tools->zoomFactor() / M_PI;
            const auto phi = std::atan2(-invCamTimesSunDir.x, invCamTimesSunDir.y);
            const auto ndcSunX = r*std::cos(phi);
            const auto ndcSunY = r*std::sin(phi);

            const auto ndcNewSunX = ndcSunX + mouseDeltaX/(width()/2.);
            const auto ndcNewSunY = ndcSunY + mouseDeltaY/(height()/2.);

            const auto newR = std::hypot(ndcNewSunX, ndcNewSunY);
            const auto sinc = newR==0 ? M_PI / tools->zoomFactor() : std::sin(newR * M_PI / tools->zoomFactor()) / newR;
            const auto newSunDir = glm::dvec3(-ndcNewSunY * sinc, ndcNewSunX * sinc, std::cos(newR * M_PI / tools->zoomFactor()));

            const auto unrotatedNewSunDir = cameraRotation * newSunDir;
            const auto newSunZenithAngle = std::acos(std::clamp(unrotatedNewSunDir.z, -1., 1.));
            const auto newSunAzimuth = std::atan2(unrotatedNewSunDir.y, unrotatedNewSunDir.x);

            tools->setSunZenithAngle(newSunZenithAngle);
            tools->setSunAzimuth(newSunAzimuth);
            break;
        }
        }
        break;
    }
    case DragMode::Camera:
    {
        const auto oldPitch=tools->cameraPitch(), oldYaw=tools->cameraYaw();
        tools->setCameraPitch(std::clamp(oldPitch + (prevMouseY_-pos.y())*M_PI/height()/tools->zoomFactor(), -M_PI/2, M_PI/2));
        tools->setCameraYaw(std::remainder(oldYaw - (prevMouseX_-pos.x())*2*M_PI/width()/tools->zoomFactor(), 2*M_PI));
        break;
    }
    default:
        break;
    }
    prevMouseX_=pos.x();
    prevMouseY_=pos.y();
    update();
}

void GLWidget::mousePressEvent(QMouseEvent* event)
{
    if(event->buttons()==Qt::LeftButton && !(event->modifiers() & (Qt::ControlModifier|Qt::ShiftModifier)))
    {
        updateSpectralRadiance(event->pos());
        return;
    }

    const auto pos = position(event);
    if(event->modifiers() & Qt::ControlModifier)
        setDragMode(DragMode::Sun, pos.x(), pos.y());
    else
        setDragMode(DragMode::Camera, pos.x(), pos.y());
}

void GLWidget::mouseReleaseEvent(QMouseEvent*)
{
    setDragMode(DragMode::None);
}

void GLWidget::keyPressEvent(QKeyEvent* event)
{
    switch(event->key())
    {
    case Qt::Key_S:
        if((event->modifiers() & (Qt::ControlModifier|Qt::ShiftModifier|Qt::AltModifier)) != Qt::ControlModifier)
            break;
        saveScreenshot();
        break;
    default:
        QOpenGLWidget::keyPressEvent(event);
        break;
    }
}

void GLWidget::saveScreenshot()
{
    const auto path=QFileDialog::getSaveFileName(this, tr("Save screenshot"), {}, "float32 image files (*.f32)");
    if(path.isNull())
        return;
    makeCurrent();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, renderer->getLuminanceTexture());
    std::vector<float> data(width()*height()*4);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, data.data());
    QFile file(path);
    if(!file.open(QFile::WriteOnly))
    {
        QMessageBox::critical(this, tr("Error saving screenshot"), tr("Failed to open destination file: %1").arg(file.errorString()));
        return;
    }
    const uint16_t width=this->width(), height=this->height();
    file.write(reinterpret_cast<const char*>(&width), sizeof width);
    file.write(reinterpret_cast<const char*>(&height), sizeof height);
    file.write(reinterpret_cast<const char*>(data.data()), data.size()*sizeof data[0]);
    if(!file.flush())
    {
        QMessageBox::critical(this, tr("Error saving screenshot"), tr("Failed to write to destination file: %1").arg(file.errorString()));
        return;
    }
}

void GLWidget::setupBuffers()
{
    if(!vao_)
        glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);
    if(!vbo_)
        glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    const GLfloat vertices[]=
    {
        -1, -1,
         1, -1,
        -1,  1,
         1,  1,
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof vertices, vertices, GL_STATIC_DRAW);
    constexpr GLuint attribIndex=0;
    constexpr int coordsPerVertex=2;
    glVertexAttribPointer(attribIndex, coordsPerVertex, GL_FLOAT, false, 0, 0);
    glEnableVertexAttribArray(attribIndex);
    glBindVertexArray(0);
}

void GLWidget::reloadShaders()
{
    if(!renderer) return;
    makeCurrent();
    renderer->initShaderReloading();
    stepShaderReloading();
}

void GLWidget::stepShaderReloading()
{
    try
    {
        makeCurrent();
        const auto status = renderer->stepShaderReloading();
        if(status.stepsToDo < 0) return;

        emit loadProgress(renderer->currentActivity(), status.stepsDone, status.stepsToDo);
        if(renderer->isReadyToRender())
            update();
        else if(status.stepsDone < status.stepsToDo)
            QTimer::singleShot(0, this, &GLWidget::stepShaderReloading);
    }
    catch(ShowMySky::Error const& ex)
    {
        QTimer::singleShot(0,
            [this,errorType=ex.errorType(),what=ex.what()]
            {
                emit loadProgress(tr("Shader reloading failed"), 0, 0);
                QMessageBox::critical(this, errorType, what);
            });
    }
}

bool GLWidget::eventFilter(QObject* object, QEvent* event)
{
    if(event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut)
    {
        // Prevent repaints due to the window becoming active or inactive. This must be combined with
        // WindowActivate/WindowDeactivate events filtered out in toplevel window.
        return true;
    }
    return QOpenGLWidget::eventFilter(object, event);
}
