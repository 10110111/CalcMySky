#include "GLWidget.hpp"
#include <chrono>
#include <QMouseEvent>
#include <QSurfaceFormat>
#include "../common/util.hpp"

GLWidget::GLWidget(std::string const& pathToData, QWidget* parent)
    : QOpenGLWidget(parent)
    , pathToData(pathToData)
{
}

void GLWidget::initializeGL()
{
    if(!initializeOpenGLFunctions())
    {
        std::cerr << "Failed to initialize OpenGL "
            << QSurfaceFormat::defaultFormat().majorVersion() << '.'
            << QSurfaceFormat::defaultFormat().minorVersion() << " functions\n";
        throw MustQuit{};
    }

    renderer.reset(new AtmosphereRenderer(*this,pathToData));
    connect(renderer.get(), &AtmosphereRenderer::sunElevationChanged, this, &GLWidget::sunElevationChanged);
    connect(renderer.get(), &AtmosphereRenderer::sunAzimuthChanged, this, &GLWidget::sunAzimuthChanged);
    emit sunElevationChanged(renderer->sunElevation());
    emit sunAzimuthChanged(renderer->sunAzimuth());
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

void GLWidget::setAltitude(const double altitude)
{
    renderer->setAltitude(altitude);
    update();
}

void GLWidget::setExposureLog(const double exposureLog)
{
    renderer->setExposure(std::pow(10.,exposureLog));
    update();
}

void GLWidget::setSunElevation(double elevationDeg)
{
    renderer->setSunElevation(elevationDeg*(M_PI/180));
    update();
}

void GLWidget::setSunAzimuth(double azimuthDeg)
{
    renderer->setSunAzimuth(azimuthDeg*(M_PI/180));
    update();
}
