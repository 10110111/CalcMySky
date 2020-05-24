#ifndef INCLUDE_ONCE_71D92E37_E297_472C_8495_1BF8EA61DC99
#define INCLUDE_ONCE_71D92E37_E297_472C_8495_1BF8EA61DC99

#include <memory>
#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include "AtmosphereRenderer.hpp"

class GLWidget : public QOpenGLWidget, public QOpenGLFunctions_3_3_Core
{
    Q_OBJECT

    std::unique_ptr<AtmosphereRenderer> renderer;
    std::string pathToData;
public:
    explicit GLWidget(std::string const& pathToData, QWidget* parent=nullptr);
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

    void setAltitude(double altitude);
    void setExposureLog(double exposureLog);
    void setFovY(double fovY);
    void setSunElevation(double elevationDeg);
    void setSunAzimuth(double azimuthDeg);

signals:
    void sunElevationChanged(double elevation);
    void sunAzimuthChanged(double azimuth);
    void frameFinished(long long timeInUS);
};

#endif
