#ifndef INCLUDE_ONCE_71D92E37_E297_472C_8495_1BF8EA61DC99
#define INCLUDE_ONCE_71D92E37_E297_472C_8495_1BF8EA61DC99

#include <memory>
#include <QOpenGLWidget>
#include <QOpenGLTexture>
#include <QOpenGLFunctions_3_3_Core>
#include "AtmosphereRenderer.hpp"
#include "../common/AtmosphereParameters.hpp"

class ToolsWidget;
class GLWidget : public QOpenGLWidget, public QOpenGLFunctions_3_3_Core
{
    Q_OBJECT

    std::unique_ptr<ShowMySky::AtmosphereRenderer> renderer;
    std::unique_ptr<QOpenGLShaderProgram> luminanceToScreenRGB_;
    QOpenGLTexture bayerPatternTexture_;
    QString pathToData;
    ToolsWidget* tools;
    GLuint vao_=0, vbo_=0;
    QPoint lastRadianceCapturePosition{-1,-1};
    decltype(::ShowMySky_AtmosphereRenderer_create)* ShowMySky_AtmosphereRenderer_create=nullptr;

    enum class DragMode
    {
        None,
        Sun,
        Camera,
    } dragMode_=DragMode::None;
    int prevMouseX_, prevMouseY_;

public:
    enum class DitheringMode
    {
        Disabled,    //!< Dithering disabled, will leave the infamous color bands
        Color565,    //!< 16-bit color (AKA High color) with R5_G6_B5 layout
        Color666,    //!< TN+film typical color depth in TrueColor mode
        Color888,    //!< 24-bit color (AKA True color)
        Color101010, //!< 30-bit color (AKA Deep color)
    };

public:
    explicit GLWidget(QString const& pathToData, ToolsWidget* tools, QWidget* parent=nullptr);
    ~GLWidget();

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
    void wheelEvent(QWheelEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void setupBuffers();
    void reloadShaders();
    QVector3D rgbMaxValue() const;
    void makeBayerPatternTexture();
    void updateSpectralRadiance(QPoint const& pixelPos);
    void setDragMode(DragMode mode, int x=0, int y=0) { dragMode_=mode; prevMouseX_=x; prevMouseY_=y; }

private slots:
    void onLoadProgress(QString const& currentActivity, int stepsDone, int stepsToDo);

signals:
    void frameFinished(long long timeInUS);
};

#endif
