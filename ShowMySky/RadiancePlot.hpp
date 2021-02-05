#ifndef INCLUDE_ONCE_BA229016_D8DA_4EB4_89B7_7F5CCD722A60
#define INCLUDE_ONCE_BA229016_D8DA_4EB4_89B7_7F5CCD722A60

#include <cmath>
#include <memory>
#include <QWidget>

class QTextDocument;
class QStatusBar;
class RadiancePlot : public QWidget
{
    QStatusBar* statusBar=nullptr;
    QTransform coordTransform;
    std::vector<float> wavelengths, radiances;
    float luminance;
    float azimuth=NAN, elevation=NAN;
    int focusedPoint=-1;
public:
    RadiancePlot(QStatusBar* statusBar, QWidget* parent=nullptr);
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
