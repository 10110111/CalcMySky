#ifndef INCLUDE_ONCE_71D92E37_E297_472C_8495_1BF8EA61DC99
#define INCLUDE_ONCE_71D92E37_E297_472C_8495_1BF8EA61DC99

#include <memory>
#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include "AtmosphereRenderer.hpp"

class ToolsWidget;
class GLWidget : public QOpenGLWidget, public QOpenGLFunctions_3_3_Core
{
    Q_OBJECT

    std::unique_ptr<AtmosphereRenderer> renderer;
    AtmosphereRenderer::Parameters params;
    QString pathToData;
    ToolsWidget* tools;
    QPoint lastRadianceCapturePosition{-1,-1};
public:
    explicit GLWidget(QString const& pathToData, AtmosphereRenderer::Parameters const& params,
                      ToolsWidget* tools, QWidget* parent=nullptr);
    ~GLWidget();

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void reloadShaders();
    void updateSpectralRadiance(QPoint const& pixelPos);

signals:
    void frameFinished(long long timeInUS);
};

#endif
