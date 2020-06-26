#include "RadiancePlot.hpp"
#include <cassert>
#include <algorithm>
#include <QPainter>
#include <QPaintEvent>
#include <QRegularExpression>

static auto backgroundColor() { return QColor(140,140,140); }
static auto curveColor() { return QColor(0x3f,0x3d,0x99); }
static auto textColor() { return QColor(255,255,255); }
constexpr double ticksTextSpaceFactor=1.2; // to avoid fitting the text too tightly
constexpr double xTickHeightFactor=1; // relative to font height
constexpr double xTickSpacingFactor=1.0;
constexpr double topMarginFactor=3;
constexpr double yTickWidthFactor=1; // relative to font 'x' width
constexpr double yTickSpacingFactor=1.5;
constexpr double rightMarginFactor=3;

RadiancePlot::RadiancePlot(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_NoSystemBackground,true);
    setWindowTitle(tr("Spectral radiance - ShowMySky"));
}

void RadiancePlot::setData(const float* wavelengths, const float* radiances, const unsigned size)
{
    this->wavelengths.resize(size);
    this->radiances.resize(size);
    std::copy_n(wavelengths, size, this->wavelengths.data());
    std::copy_n(radiances, size, this->radiances.data());
    update();
}

QMarginsF RadiancePlot::calcPlotMargins(QPainter const& p, std::vector<std::pair<float,QString>> const& ticksY) const
{
    const QFontMetricsF fm(p.font());
    double left=0;
    for(const auto& [_,tick] : ticksY)
        if(const auto w=fm.width(tick); left<w)
            left=w;
    left *= ticksTextSpaceFactor;
    left += fm.width('x')*(yTickWidthFactor+yTickSpacingFactor);
    const double bottom = fm.height()*ticksTextSpaceFactor + fm.height()*(xTickHeightFactor+xTickSpacingFactor);
    const double top=fm.height()*topMarginFactor, right=fm.width('x')*rightMarginFactor;
    return {left,top,right,bottom};
}

void RadiancePlot::drawAxes(QPainter& p, std::vector<std::pair<float,QString>> const& ticksX,
                            std::vector<std::pair<float,QString>> const& ticksY,
                            const float xMin, const float xMax, const float yMin, const float yMax) const
{
    p.save();
    p.setPen(textColor());
    auto m=p.transform();
    p.resetTransform();
    const QFontMetricsF fm(p.font());
    {
        const auto textMarginLeft=fm.width('x')*yTickSpacingFactor/2;
        const auto axisPos=m.dx()+m.m11()*xMin;
        for(const auto& [y, label] : ticksY)
        {
            p.drawText(QPointF(textMarginLeft, m.dy()+m.m22()*y+fm.height()/2), label);
            p.drawLine(QPointF(axisPos-textMarginLeft, m.dy()+m.m22()*y), QPointF(axisPos, m.dy()+m.m22()*y));
        }
        p.drawLine(QPointF(axisPos, m.dy()+m.m22()*yMin), QPointF(axisPos, m.dy()+m.m22()*yMax));
    }
    {
        const auto textMarginBottom=fm.height()*xTickSpacingFactor/2;
        const auto axisPos=m.dy();
        for(const auto& [x, label] : ticksX)
        {
            p.drawText(QPointF(m.dx()+m.m11()*x-fm.width(label)/2, height()-1-textMarginBottom), label);
            p.drawLine(QPointF(m.dx()+m.m11()*x, axisPos+textMarginBottom), QPointF(m.dx()+m.m11()*x, axisPos));
        }
        p.drawLine(QPointF(m.dx()+m.m11()*xMin, axisPos), QPointF(m.dx()+m.m11()*xMax, axisPos));
    }
    p.restore();
}

static std::vector<float> generateTicks(const float min, const float max)
{
    using namespace std;
    const auto range=max-min;
    // "Head" is two most significant digits, tail is the remaining digits
    // of the whole part (tail length is negative if |range|<1)
    const auto rangeTailLen=floor(log10(range))-1;
    const auto scale=pow(10.,rangeTailLen);
    const int headOfRange=floor(range/scale);
    const int headOfMin=floor(abs(min)/scale);
    const int headOfMax=floor(abs(max)/scale);

    const int step = headOfRange>=50 ? 10 :
                     headOfRange>=25 ?  5 :
                                        2;
    std::vector<float> ticks;
    // Round head of lowest-value tick, so that it has nicer tail WRT the actual step size.
    // This may result in losing the tick below it if this is done by simple rounding, so it's
    // additionally decreased by step size to prevent this. In actual generating loop we simply
    // check that the tick is not invisible due to the decrease.
    const auto initTickHead=(min<0?-1:1)*headOfMin/step*step-step;
    for(int head=initTickHead; head<=headOfMax; head+=step)
    {
        const auto v=head*scale;
        if(v>=min) // might not be so due to our rounding of initial value
            ticks.push_back(v);
    }
    return ticks;
}

static QString formatTick(const float value)
{
    return QString::number(value, 'g', 5);
}

std::vector<std::pair<float,QString>> RadiancePlot::genTicks(std::vector<float> const& points, const float min) const
{
    std::vector<std::pair<float,QString>> output;
    const auto minmax=std::minmax_element(points.begin(), points.end());
    const auto tickValues=generateTicks(std::isnan(min) ? *minmax.first : min, *minmax.second);
    for(const auto v : tickValues)
        output.push_back({v,formatTick(v)});
    return output;
}

void RadiancePlot::setupQPainter(QPainter& p) const
{
    assert(!wavelengths.empty());

    const auto ticksX=genTicks(wavelengths);
    const auto ticksY=genTicks(radiances, 0);

    const float pixMin=0, pixMax=*std::max_element(radiances.begin(), radiances.end());
    const float w=width(), h=height();
    const float wlMin=wavelengths.front(), wlMax=wavelengths.back();
    const auto margins=calcPlotMargins(p, ticksY);
    const float marginLeft=margins.left(), marginRight=margins.right(),
                marginTop=margins.top(), marginBottom=margins.bottom();

    /* These are solution to
     * {{{wlMax,pixMax,1}}.{{sx,0,0},{0,sy,0},{dx,dy,1}}=={{w-1-marginRight,marginTop,1}},
     *  {{wlMin,pixMin,1}}.{{sx,0,0},{0,sy,0},{dx,dy,1}}=={{marginLeft,h-1-marginBottom,1}}}
     */
    const float sx=(1 + marginLeft + marginRight - w)/(-wlMax + wlMin);
    const float sy=(1 - h + marginBottom + marginTop)/(pixMax - pixMin);
    const float dx=(marginLeft*wlMax + wlMin + marginRight*wlMin - w*wlMin)/(wlMax - wlMin);
    const float dy=(pixMax - h*pixMax + marginBottom*pixMax + marginTop*pixMin)/(pixMin-pixMax);
    p.setTransform(QTransform(sx,0,0,sy,dx,dy));

    drawAxes(p, ticksX, ticksY, wlMin, wlMax, pixMin, pixMax);

    p.setRenderHint(QPainter::Antialiasing,true);
}

void RadiancePlot::paintEvent(QPaintEvent *event)
{
    QPainter p(this);
    p.fillRect(event->rect(),backgroundColor());

    if(wavelengths.empty())
    {
        p.setPen(textColor());
        p.drawText(rect(), Qt::AlignCenter|Qt::AlignHCenter|Qt::TextWordWrap,
                   tr("Click on the image in the main window to see spectral radiance of a point"));
        return;
    }

    setupQPainter(p);

    QPainterPath path;
    path.moveTo(wavelengths.front(), radiances.front());
    for(size_t i=1;i<wavelengths.size();++i)
        path.lineTo(wavelengths[i],radiances[i]);

    p.setPen(QPen(curveColor(),0));
    p.drawPath(path);

    // close path to fill space under the curve
    path.lineTo(wavelengths.back(),0);
    path.lineTo(wavelengths.front(),0);

    auto fillBrush=curveColor();
    fillBrush.setAlphaF(0.3);
    p.fillPath(path,fillBrush);
}
