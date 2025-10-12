/*
 * CalcMySky - a simulator of light scattering in planetary atmospheres
 * Copyright © 2025 Ruslan Kabatsayev
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335, USA.
 */

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

public:
    enum class Projection
    {
        Equirectangular,
        Perspective,
        Fisheye,
        Cubemap,
    };
    enum class ColorMode
    {
        sRGB,
        ScotopicLuminance,
        PhotopicLuminance,
        XYZChromaticity,
        sRGBlChromaticity,
        sRGBlChromaticityToMax,
        sRGB_Red,
        sRGB_Green,
        sRGB_Blue,
    };

private:
    std::unique_ptr<ShowMySky::AtmosphereRenderer> renderer;
    std::unique_ptr<QOpenGLShaderProgram> luminanceToScreenRGB_;
    std::unique_ptr<QOpenGLShaderProgram> glareProgram_;
    QOpenGLTexture ditherPatternTexture_;
    GLuint glareTextures_[2] = {};
    GLuint glareFBOs_[2] = {};
    QString pathToData;
    ToolsWidget* tools;
    GLuint vao_=0, vbo_=0;
    QPoint lastRadianceCapturePosition{-1,-1};
    decltype(::ShowMySky_AtmosphereRenderer_create)* ShowMySky_AtmosphereRenderer_create=nullptr;
    Projection currentProjection_ = Projection::Equirectangular;
    ColorMode currentColorMode_ = ColorMode::sRGB;

    enum class DragMode
    {
        None,
        Sun,
        Camera,
    } dragMode_=DragMode::None;
    double prevMouseX_, prevMouseY_;

public:
    enum class DitheringMode
    {
        Color565,    //!< 16-bit color (AKA High color) with R5_G6_B5 layout
        Color666,    //!< TN+film typical color depth in TrueColor mode
        Color888,    //!< 24-bit color (AKA True color)
        Color101010, //!< 30-bit color (AKA Deep color)
    };
    enum class DitheringMethod
    {
        NoDithering,                //!< Dithering disabled, will leave the infamous color bands
        Bayer,                      //!< Ordered dithering using Bayer threshold texture
        BlueNoiseTriangleRemapped,  //!< Unordered dithering using blue noise of amplitude 1.0, with triangular remapping
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
    bool eventFilter(QObject* object, QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    int width() const;
    int height() const;
    void setupBuffers();
    void reloadShaders();
    void stepDataLoading();
    void stepShaderReloading();
    void stepPreparationToDraw(bool emitProgressStatus);
    QVector3D rgbMaxValue() const;
    void makeGlareRenderTarget();
    void makeDitherPatternTexture();
    void updateSpectralRadiance(QPoint const& pixelPos);
    void setDragMode(DragMode mode, double x=0, double y=0) { dragMode_=mode; prevMouseX_=x; prevMouseY_=y; }
    void setFlatSolarSpectrum();
    void resetSolarSpectrum();
    void setBlackBodySolarSpectrum(double temperature);
    void saveScreenshot();
    Projection currentProjection() const { return currentProjection_; }
    ColorMode  currentColorMode () const { return currentColorMode_; }

signals:
    void frameFinished(long long timeInUS);
    void loadProgress(QString const& currentActivity, int stepsDone, int stepsToDo);
};

#endif
