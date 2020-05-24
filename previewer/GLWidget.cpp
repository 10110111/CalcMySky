#include "GLWidget.hpp"
#include <chrono>
#include <QMouseEvent>
#include <QSurfaceFormat>
#include "../common/util.hpp"
#include "util.hpp"
#include "ToolsWidget.hpp"

GLWidget::GLWidget(QString const& pathToData, AtmosphereRenderer::Parameters const& params, ToolsWidget* tools, QWidget* parent)
    : QOpenGLWidget(parent)
    , params(params)
    , pathToData(pathToData)
    , tools(tools)
{
    connect(this, &GLWidget::frameFinished, tools, &ToolsWidget::showFrameRate);
}

GLWidget::~GLWidget()
{
    // Let the destructor of renderer have current GL context. This avoids warnings from QOpenGLTexturePrivate::destroy().
    makeCurrent();
}

void GLWidget::initializeGL()
{
    if(!initializeOpenGLFunctions())
    {
        throw InitializationError{tr("Failed to initialize OpenGL %1.%2 functions")
                                    .arg(QSurfaceFormat::defaultFormat().majorVersion())
                                    .arg(QSurfaceFormat::defaultFormat().minorVersion())};
    }

    renderer.reset(new AtmosphereRenderer(*this,pathToData,params,tools));
    const auto update=qOverload<>(&GLWidget::update);
    connect(renderer.get(), &AtmosphereRenderer::needRedraw, this, update);
    connect(tools, &ToolsWidget::settingChanged, this, update);
    connect(tools, &ToolsWidget::setScattererEnabled, renderer.get(), &AtmosphereRenderer::setScattererEnabled);
    connect(tools, &ToolsWidget::reloadShadersClicked, this, &GLWidget::reloadShaders);
}

void GLWidget::paintGL()
{
    const auto t0=std::chrono::steady_clock::now();
    renderer->draw();
    glFinish();
    const auto t1=std::chrono::steady_clock::now();
    emit frameFinished(std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count());
}

void GLWidget::resizeGL(int w, int h)
{
    renderer->resizeEvent(w,h);
}

void GLWidget::mouseMoveEvent(QMouseEvent* event)
{
    renderer->mouseMove(event->x(), event->y());
    update();
}

void GLWidget::mousePressEvent(QMouseEvent* event)
{
    if(event->modifiers() & Qt::ControlModifier)
        renderer->setDragMode(AtmosphereRenderer::DragMode::Sun, event->x(), event->y());
    else
        renderer->setDragMode(AtmosphereRenderer::DragMode::Camera, event->x(), event->y());
}

void GLWidget::mouseReleaseEvent(QMouseEvent*)
{
    renderer->setDragMode(AtmosphereRenderer::DragMode::None);
}

void GLWidget::reloadShaders()
{
    makeCurrent();
    renderer->reloadShaders(pathToData);
    update();
}
