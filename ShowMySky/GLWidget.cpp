#include "GLWidget.hpp"
#include <chrono>
#include <QMouseEvent>
#include <QMessageBox>
#include <QSurfaceFormat>
#include "../common/util.hpp"
#include "util.hpp"
#include "ToolsWidget.hpp"
#include "AtmosphereRenderer.hpp"

GLWidget::GLWidget(QString const& pathToData, ToolsWidget* tools, QWidget* parent)
    : QOpenGLWidget(parent)
    , bayerPatternTexture_(QOpenGLTexture::Target2D)
    , pathToData(pathToData)
    , tools(tools)
{
    connect(this, &GLWidget::frameFinished, tools, &ToolsWidget::showFrameRate);
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
}

void GLWidget::makeBayerPatternTexture()
{
    bayerPatternTexture_.setMinificationFilter(QOpenGLTexture::Nearest);
    bayerPatternTexture_.setMagnificationFilter(QOpenGLTexture::Nearest);
    bayerPatternTexture_.setWrapMode(QOpenGLTexture::Repeat);
    bayerPatternTexture_.bind();
	static constexpr float bayerPattern[8*8] =
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
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 8, 8, 0, GL_RED, GL_FLOAT, bayerPattern);
}

QVector3D GLWidget::rgbMaxValue() const
{
    switch(tools->ditheringMode())
	{
		default:
		case DitheringMode::Disabled:
			return QVector3D(0,0,0);
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
        renderer.reset(ShowMySky_AtmosphereRenderer_create(this,&pathToData,tools));
        tools->updateParameters(static_cast<AtmosphereRenderer*>(renderer.get())->atmosphereParameters());
        connect(renderer->asQObject(), SIGNAL(loadProgress(QString const&,int,int)), this, SLOT(onLoadProgress(QString const&,int,int)));
        connect(tools, &ToolsWidget::settingChanged, this, qOverload<>(&GLWidget::update));
        connect(tools, &ToolsWidget::setScattererEnabled, this, [this,renderer=renderer.get()](QString const& name, const bool enable)
                { renderer->setScattererEnabled(name, enable); update(); });
        connect(tools, &ToolsWidget::reloadShadersClicked, this, &GLWidget::reloadShaders);

        makeBayerPatternTexture();
        setupBuffers();

        luminanceToScreenRGB_=std::make_unique<QOpenGLShaderProgram>();
        addShaderCode(*luminanceToScreenRGB_, QOpenGLShader::Fragment, tr("luminanceToScreenRGB fragment shader"), 1+R"(
#version 330
uniform float exposure;
uniform sampler2D luminanceXYZW;
in vec2 texCoord;
out vec4 color;

uniform bool gradualClipping;
uniform vec3 rgbMaxValue;
uniform sampler2D bayerPattern;
vec3 dither(vec3 c)
{
    if(rgbMaxValue.r==0.) return c;
    vec3 bayer=texture2D(bayerPattern,gl_FragCoord.xy/8.).rrr;

    vec3 rgb=c*rgbMaxValue;
    vec3 head=floor(rgb);
    vec3 tail=rgb-head;
    return (head+1.-step(tail,bayer))/rgbMaxValue;
}

vec3 clip(vec3 rgb)
{
    return sqrt(tanh(rgb*rgb));
}

void main()
{
    vec3 XYZ=texture(luminanceXYZW, texCoord).xyz;
    const mat3 XYZ2sRGBl=mat3(vec3(3.2406,-0.9689,0.0557),
                              vec3(-1.5372,1.8758,-0.204),
                              vec3(-0.4986,0.0415,1.057));
    vec3 rgb=XYZ2sRGBl*XYZ*exposure;
    vec3 clippedRGB = gradualClipping ? clip(rgb) : clamp(rgb, 0., 1.);
    vec3 srgb=pow(clippedRGB, vec3(1/2.2));
    color=vec4(dither(srgb),1);
}
)");
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
        static constexpr const char* viewDirFragShaderSrc=1+R"(
#version 330
in vec3 position;
uniform float zoomFactor;
const float PI=3.1415926535897932;
vec3 calcViewDir()
{
    vec2 pos=position.xy/zoomFactor;
    return vec3(cos(pos.x*PI)*cos(pos.y*(PI/2)),
                sin(pos.x*PI)*cos(pos.y*(PI/2)),
                sin(pos.y*(PI/2)));
}
)";
        renderer->loadData(viewDirVertShaderSrc, viewDirFragShaderSrc,
                           [tools=tools](QOpenGLShaderProgram& program)
                           { program.setUniformValue("zoomFactor", tools->zoomFactor()); });
        if(renderer->readyToRender())
            tools->setCanGrabRadiance(renderer->canGrabRadiance());
    }
    catch(Error const& ex)
    {
        QMessageBox::critical(this, ex.errorType(), ex.what());
    }
}

void GLWidget::onLoadProgress(QString const& currentActivity, const int stepsDone, const int stepsToDo)
{
    tools->onLoadProgress(currentActivity,stepsDone,stepsToDo);
    // Processing of load progress has likely drawn something on some widgets,
    // which would take away OpenGL context, so we must take it back.
    makeCurrent();
}

void GLWidget::paintGL()
{
    if(!renderer) return;
    if(!isVisible()) return;
    if(!renderer->readyToRender()) return;

    const auto t0=std::chrono::steady_clock::now();
    renderer->draw();

    glBindVertexArray(vao_);
    luminanceToScreenRGB_->bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, renderer->getLuminanceTexture());
    luminanceToScreenRGB_->setUniformValue("luminanceXYZW", 0);
    bayerPatternTexture_.bind(1);
    luminanceToScreenRGB_->setUniformValue("bayerPattern", 1);
    luminanceToScreenRGB_->setUniformValue("rgbMaxValue", rgbMaxValue());
    luminanceToScreenRGB_->setUniformValue("gradualClipping", tools->gradualClippingEnabled());
    luminanceToScreenRGB_->setUniformValue("exposure", tools->exposure());
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

void GLWidget::mouseMoveEvent(QMouseEvent* event)
{
    if(event->buttons()==Qt::LeftButton && !(event->modifiers() & (Qt::ControlModifier|Qt::ShiftModifier)))
    {
        updateSpectralRadiance(event->pos());
        return;
    }

    constexpr double scale=500;
    switch(dragMode_)
    {
    case DragMode::Sun:
    {
        const auto oldZA=tools->sunZenithAngle(), oldAz=tools->sunAzimuth();
        tools->setSunZenithAngle(std::clamp(oldZA - (prevMouseY_-event->y())/scale, 0., M_PI));
        tools->setSunAzimuth(std::clamp(oldAz - (prevMouseX_-event->x())/scale, -M_PI, M_PI));
        break;
    }
    default:
        break;
    }
    prevMouseX_=event->x();
    prevMouseY_=event->y();
    update();
}

void GLWidget::mousePressEvent(QMouseEvent* event)
{
    if(event->buttons()==Qt::LeftButton && !(event->modifiers() & (Qt::ControlModifier|Qt::ShiftModifier)))
    {
        updateSpectralRadiance(event->pos());
        return;
    }

    if(event->modifiers() & Qt::ControlModifier)
        setDragMode(DragMode::Sun, event->x(), event->y());
    else
        setDragMode(DragMode::Camera, event->x(), event->y());
}

void GLWidget::mouseReleaseEvent(QMouseEvent*)
{
    setDragMode(DragMode::None);
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
    renderer->reloadShaders();
    update();
}
