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

#ifndef INCLUDE_ONCE_BA229016_D8DA_4EB4_89B7_7F5CCD722A60
#define INCLUDE_ONCE_BA229016_D8DA_4EB4_89B7_7F5CCD722A60

#include <cmath>
#include <memory>
#include <QWidget>

class QLabel;
class QTextDocument;
class RadiancePlot : public QWidget
{
    QLabel* statusBar=nullptr;
    QTransform coordTransform;
    std::vector<float> wavelengths, radiances;
    float luminance;
    float azimuth=NAN, elevation=NAN;
    int focusedPoint=-1;
public:
    RadiancePlot(QLabel* statusBar, QWidget* parent=nullptr);
    void setData(const float* wavelengths, const float* radiances, unsigned size,
                 float azimuth, float elevation);

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QMarginsF calcPlotMargins(QPainter const& p, std::vector<std::pair<float,QString>> const& ticksY) const;
    void drawAxes(QPainter& p, std::vector<std::pair<float,QString>> const& ticksX,
                  std::vector<std::pair<float,QString>> const& ticksY,
                  float xMin, float xMax, float yMin, float yMax) const;
    std::vector<std::pair<float,QString>> genTicks(std::vector<float> const& values, const float min=NAN) const;
    std::unique_ptr<QTextDocument> makeQTextDoc() const;
    void saveSpectrum();
    float calcLuminance() const;
};

#endif
