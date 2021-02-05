#include "RadiancePlot.hpp"
#include <cassert>
#include <algorithm>
#include <QPainter>
#include <QStatusBar>
#include <QMessageBox>
#include <QPaintEvent>
#include <QFileDialog>
#include <QTextStream>
#include <QTextDocument>
#include <QRegularExpression>
#include "../common/cie-xyzw-functions.hpp"

static auto curveColor() { return QColor(0x3f,0x3d,0x99); }
static auto textColor() { return QColor(255,255,255); }
// These tick metrics are in units of font size (height for x ticks, 'x' width for y ticks)
constexpr double xTickSpaceUnderLabel=0.3;
constexpr double xTickSpaceAboveLabel=0.3;
constexpr double xTickLineLength=0.5;
constexpr double yTickSpaceLeft=1.0;
constexpr double yTickSpaceRight=0.7;
constexpr double yTickLineLength=1;
constexpr double yAxisSpaceLeftOfLabel=0.5;
constexpr double yAxisSpaceAboveLabel=0.5;
constexpr double topMargin=2;
constexpr double rightMargin=3;

glm::vec3 RGB2sRGB(glm::vec3 const& RGB)
{
    // No, we aren't gonna do the piecewise sRGB gamma
    // pedantry. Just the usual good approximation.
    return pow(RGB, glm::vec3(1/2.2));
}

// Linear part of sRGB transformation
glm::vec3 XYZ2RGB(glm::vec3 const& XYZ)
{
    using namespace glm;
    return dmat3(vec3(3.2406,-0.9689,0.0557),
                 vec3(-1.5372,1.8758,-0.204),
                 vec3(-0.4986,0.0415,1.057))*XYZ;
}

glm::vec3 wavelengthToRGB(const float wavelength)
{
    const auto XYZ=glm::vec3(wavelengthToXYZW(wavelength));
    return XYZ2RGB(XYZ);
}

QColor wavelengthToQColor(const float wavelength)
{
    const auto RGB=wavelengthToRGB(wavelength);

    static float rgbMin=0;
    static float rgbMax=0;
    [[maybe_unused]] static const bool inited=[]
    {
        for(double wl=400;wl<700;wl+=0.1)
        {
            const auto RGB=wavelengthToRGB(wl);
            const auto newMin=std::min({RGB.r,RGB.g,RGB.b});
            const auto newMax=std::max({RGB.r,RGB.g,RGB.b});
            if(newMin<rgbMin)
                rgbMin=newMin;
            if(newMax>rgbMax)
                rgbMax=newMax;
        }
        return true;
    }();

    const auto desaturated=(RGB-rgbMin)/(rgbMax-rgbMin); // desaturate and scale to [0,1]
    const auto sRGB=RGB2sRGB(desaturated);
    return QColor::fromRgbF(sRGB.r,sRGB.g,sRGB.b);
}

static auto backgroundColor() { return wavelengthToQColor(1000/*nm*/); }

static QBrush makeSpectrumBrush()
{
    const qreal dl=0.01;
    const qreal wlUV=360;
    const qreal wlIR=830;
    QLinearGradient gradient(QPointF(wlUV-dl,0),QPointF(wlIR+dl,0));
    gradient.setColorAt(0.,QColor(0,0,0,0));
    gradient.setColorAt(1.,QColor(0,0,0,0));
    for(qreal wavelength=wlUV;wavelength<=wlIR;wavelength+=5)
        gradient.setColorAt((wavelength-(wlUV-dl))/(wlIR+dl-(wlUV-dl)),wavelengthToQColor(wavelength));
    gradient.setSpread(QGradient::PadSpread);
    return gradient;
}

static QString eNotationToTenNotation(QString num)
{
    num.replace(QRegularExpression("^(-?[0-9](?:\\.[0-9]+)?)e\\+?(-?)0?([0-9]+)$"), "\\1&times;10<sup>\\2\\3</sup>");
    return num;
}

static float minValidValue(const float* begin, const float* end)
{
    float min=INFINITY;
    for(auto p=begin; p!=end; ++p)
        if(std::isfinite(*p) && *p<min)
            min=*p;
    if(std::isinf(min))
        return NAN;
    return min;
}

static float maxValidValue(const float* begin, const float* end)
{
    float max=-INFINITY;
    for(auto p=begin; p!=end; ++p)
        if(std::isfinite(*p) && *p>max)
            max=*p;
    if(std::isinf(max))
        return NAN;
    return max;
}

RadiancePlot::RadiancePlot(QStatusBar* statusBar, QWidget* parent)
    : QWidget(parent)
    , statusBar(statusBar)
{
    setAttribute(Qt::WA_NoSystemBackground,true);
    setMouseTracking(true);
}

void RadiancePlot::setData(const float* wavelengths, const float* radiances, const unsigned size,
                           const float azimuth, const float elevation)
{
    this->wavelengths.resize(size);
    this->radiances.resize(size);
    std::copy_n(wavelengths, size, this->wavelengths.data());
    std::copy_n(radiances, size, this->radiances.data());
    this->azimuth=azimuth;
    this->elevation=elevation;

    this->luminance=calcLuminance();

    update();
}

float RadiancePlot::calcLuminance() const
{
    float lum=0;
    // Using midpoint rule
    for(unsigned i=1; i<wavelengths.size(); ++i)
    {
        const auto lambda = (wavelengths[i] + wavelengths[i-1]) / 2;
        const auto dlambda = wavelengths[i] - wavelengths[i-1];
        const auto meanRadiance = (radiances[i] + radiances[i-1]) / 2;
        const auto spectralLuminance = 683.002f * wavelengthToXYZW(lambda).y;
        lum += spectralLuminance*meanRadiance*dlambda;
    }
    return lum;
}

QMarginsF RadiancePlot::calcPlotMargins(QPainter const& p, std::vector<std::pair<float,QString>> const& ticksY) const
{
    auto td=makeQTextDoc();
    double maxYTickLabelWidth=0;
    for(const auto& [_,tick] : ticksY)
    {
        td->setHtml(tick);
        if(const auto w=td->size().width(); maxYTickLabelWidth<w)
            maxYTickLabelWidth=w;
    }
    const QFontMetricsF fm(p.font());
    const auto left   = (yTickSpaceLeft+yTickSpaceRight+yTickLineLength) * fm.width('x')+maxYTickLabelWidth;
    const auto bottom = fm.height()*(xTickSpaceUnderLabel+1+xTickSpaceAboveLabel+xTickLineLength);
    const auto top    = fm.height()*topMargin;
    const auto right  = fm.width('x')*rightMargin;
    return {left,top,right,bottom};
}

std::unique_ptr<QTextDocument> RadiancePlot::makeQTextDoc() const
{
    auto td=std::make_unique<QTextDocument>();
    td->setDefaultFont(font());
    td->setDocumentMargin(0);
    td->setDefaultStyleSheet(QString("body{color: %1;}").arg(textColor().name()));
    return td;
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
    const auto charWidth=fm.width('x');
    const auto td=makeQTextDoc();
    // Y ticks
    {
        const auto axisPos=m.dx()+m.m11()*xMin;
        for(const auto& [y, label] : ticksY)
        {
            const auto tickY = m.dy()+m.m22()*y;

            td->setHtml(label);
            // Right-justifying the label
            const auto labelX = axisPos - (yTickLineLength+yTickSpaceRight)*charWidth - td->size().width();
            p.save();
             p.translate(labelX, tickY - td->size().height()/2);
             td->drawContents(&p);
            p.restore();

            p.drawLine(QPointF(axisPos-yTickLineLength*charWidth, tickY),
                       QPointF(axisPos, tickY));
        }
        p.drawLine(QPointF(axisPos, m.dy()+m.m22()*yMin), QPointF(axisPos, m.dy()+m.m22()*yMax));
        td->setHtml(tr("<body>radiance,\nW&middot;m<sup>-2</sup>&#8239;sr<sup>-1</sup>&#8239;nm<sup>-1</sup>,"
                       " at azimuth %1&deg;, elevation %2&deg;</body>")
                    .arg(azimuth).arg(elevation));
        p.save();
         p.translate(yAxisSpaceLeftOfLabel*charWidth, yAxisSpaceAboveLabel*fm.height());
         td->drawContents(&p);
        p.restore();

        const auto axisLabelWidth = td->size().width();
        const auto lumFormatted = eNotationToTenNotation(QString::number(luminance, 'g', 4));
        td->setHtml(tr("<body>luminance: %1 cd&#x2215;m<sup>2</sup></body>").arg(lumFormatted));
        const auto luminanceLabelWidth = td->size().width() + rightMargin*charWidth;
        if(axisLabelWidth + luminanceLabelWidth + 5*charWidth < width())
        {
            p.save();
             p.translate(width() - luminanceLabelWidth, yAxisSpaceAboveLabel*fm.height());
             td->drawContents(&p);
            p.restore();
        }
    }
    // X ticks
    {
        const auto axisPos=m.dy();
        const auto tickBottomY = axisPos + xTickLineLength*fm.height();
        const auto labelPosY = tickBottomY + fm.height()*xTickSpaceAboveLabel;
        for(const auto& [x, label] : ticksX)
        {
            const auto tickX = m.dx()+m.m11()*x;
            td->setHtml(label);
            p.save();
             p.translate(tickX - td->size().width()/2, labelPosY);
             td->drawContents(&p);
            p.restore();
            p.drawLine(QPointF(tickX, tickBottomY), QPointF(tickX, axisPos));
        }
        p.drawLine(QPointF(m.dx()+m.m11()*xMin, axisPos), QPointF(m.dx()+m.m11()*xMax, axisPos));
        td->setHtml(tr(u8"<body>\u03bb, nm</body>"));
        // FIXME: this choice of X coordinate can overlap with the rightmost tick label
        p.save();
         p.translate(m.dx()+m.m11()*xMax - td->size().width()/2, labelPosY);
         td->drawContents(&p);
        p.restore();
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

std::vector<std::pair<float,QString>> RadiancePlot::genTicks(std::vector<float> const& points, const float min) const
{
    std::vector<std::pair<float,QString>> output;
    const auto minVal=minValidValue(points.data(), points.data()+points.size());
    const auto maxVal=maxValidValue(points.data(), points.data()+points.size());
    const auto tickValues=generateTicks(std::isnan(min) ? minVal : min, maxVal);
    for(const auto v : tickValues)
    {
        const auto num = std::abs(v);

        auto formatted=QString::number(num, 'g', 5);
        // Special case: avoid 0.000xyz in favor of x.yze-4, and 0.000x in favor of xe-4
        formatted.replace(QRegularExpression("^0\\.000([0-9])([0-9]+)$"), "\\1.\\2e-4");
        formatted.replace(QRegularExpression("^0\\.000([0-9])$"), "\\1e-4");

        output.push_back({v,formatted});
    }

	{
		// Make sequences like {2.5, 3, 3.5, 4} have consistent number of digits
		bool haveOneDigit=false, haveTwoDigit=false;
		for(const auto& [_, tick] : output)
		{
			if(tick.contains(QRegularExpression("^[0-9](?:e.*)?$")))
				haveOneDigit=true;
			if(tick.contains(QRegularExpression("^[0-9]\\.[0-9](?:e.*)?$")))
				haveTwoDigit=true;
		}
		if(haveOneDigit && haveTwoDigit)
		{
			for(auto& [_, tick] : output)
			{
				if(!tick.contains(QRegularExpression("^[0-9](?:e.*)?$")))
					continue;
				tick.insert(1, ".0");
			}
		}
	}
	{
		// Make the sequences like {0.02, 0.025, 0.03, 0.035} have consistent number of digits
		int longestWithZeroHead=0;
		for(const auto& [_, tick] : output)
		{
			if(tick.contains(QLatin1Char('e')))
				continue;
			if(tick.startsWith("0.") && longestWithZeroHead < tick.size())
				longestWithZeroHead=tick.size();
		}
		for(auto& [_, tick] : output)
		{
			if(tick.contains(QLatin1Char('e')))
				continue;
			if(tick.startsWith("0.") && tick.size() < longestWithZeroHead)
				tick.append(QLatin1Char('0'));
		}
	}

    for(auto& [value, tick] : output)
    {
        tick=eNotationToTenNotation(tick);
        tick = QString("<body>%1%2</body>").arg(value<0?"-":"", tick);
    }

    return output;
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

    assert(!wavelengths.empty());
    const auto ticksX=genTicks(wavelengths);
    const auto ticksY=genTicks(radiances, 0);

    const float pixMin=0;
    float pixMax=maxValidValue(radiances.data(), radiances.data()+radiances.size());
    if(pixMax==0 || std::isnan(pixMax))
        pixMax=1;
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
    coordTransform=QTransform(sx,0,0,sy,dx,dy);
    p.setTransform(coordTransform);

    p.setRenderHint(QPainter::Antialiasing,true);

    QPainterPath curve;
    curve.moveTo(wavelengths.front(), radiances.front());
    for(size_t i=1;i<wavelengths.size();++i)
        curve.lineTo(wavelengths[i],radiances[i]);

    auto filling=curve;
    // close the path to fill space under the curve
    filling.lineTo(wavelengths.back(),1e-30); // y!=0, because for some reason QPainter ignores this point when all the values are very small, and this ordinate is 0
    filling.lineTo(wavelengths.front(),0);

    const auto fillBrush=makeSpectrumBrush();
    p.fillPath(filling,fillBrush);

    p.setPen(QPen(curveColor(),0));
    p.drawPath(curve);

    // Mark singular values: infinities or NaNs
    {
        const auto badValueMark=QColor(127,0,0);
        const float markSizePx=QFontMetricsF(p.font()).width("W");
        const auto markSizeX=markSizePx/sx;
        const auto invXform = coordTransform.inverted();
        const auto top      = invXform.map(QPointF(0,height())).y();
        const auto bottom   = invXform.map(QPointF(0,0)).y();
        for(size_t i=0;i<wavelengths.size();++i)
        {
            const auto y=radiances[i];
            if(!std::isfinite(y))
            {
                p.setPen(QPen(backgroundColor(), 0));
                p.setBrush(backgroundColor());
                const auto wlLeft = i==0 ? wavelengths.front() : (wavelengths[i-1]+wavelengths[i])/2;
                const auto wlRight = i==wavelengths.size()-1 ? wavelengths.back() : (wavelengths[i+1]+wavelengths[i])/2;
                p.drawRect(QRectF(QPointF(wlLeft, top), QPointF(wlRight, bottom)));
                p.setPen(QPen(badValueMark, 0));
                p.setBrush(badValueMark);
            }
            if(std::isinf(y))
            {
                if(y>0)
                {
                    p.drawRect(QRectF(QPointF(wavelengths[i]-markSizeX/2, pixMax),
                                      QPointF(wavelengths[i]+markSizeX/2, pixMax*0.9+pixMin*0.1)));
                }
                else
                {
                    p.drawRect(QRectF(QPointF(wavelengths[i]-markSizeX/2, pixMin),
                                      QPointF(wavelengths[i]+markSizeX/2, pixMin*0.9+pixMax*0.1)));
                }
            }
            else if(std::isnan(y))
            {
                p.drawRect(QRectF(QPointF(wavelengths[i]-markSizeX/2, pixMin),
                                  QPointF(wavelengths[i]+markSizeX/2, pixMax)));
            }
        }
    }

    p.setRenderHint(QPainter::Antialiasing,false);
    drawAxes(p, ticksX, ticksY, wlMin, wlMax, pixMin, pixMax);
}

void RadiancePlot::keyPressEvent(QKeyEvent* event)
{
    if(event->key()==Qt::Key_S &&
       (event->modifiers() & (Qt::ControlModifier|Qt::ShiftModifier|Qt::AltModifier))==Qt::ControlModifier)
    {
        saveSpectrum();
    }
}

void RadiancePlot::mouseMoveEvent(QMouseEvent* event)
{
    if(wavelengths.empty())
    {
        statusBar->clearMessage();
        return;
    }
    const auto pos=coordTransform.inverted().map(QPointF(event->pos()));
    statusBar->showMessage(QString("(%1, %2)").arg(pos.x()).arg(pos.y()));
}

void RadiancePlot::leaveEvent(QEvent*)
{
    statusBar->clearMessage();
}

void RadiancePlot::saveSpectrum()
{
    if(wavelengths.empty()) return;

    const auto path=QFileDialog::getSaveFileName(this, tr("Save spectrum"), {}, tr("CSV tables (*.csv)"));
    if(path.isNull()) return;

    QFile file(path);
    if(!file.open(QFile::WriteOnly))
    {
        QMessageBox::critical(this, tr("Failed to open file"),
                              tr("Failed to open destination file: %1").arg(file.errorString()));
        return;
    }
    QTextStream s(&file);
    s << "wavelength (nm),radiance (W/m^2/sr/nm)\n";
    for(unsigned i=0; i<wavelengths.size(); ++i)
        s << wavelengths[i] << "," << radiances[i] << "\n";
}
