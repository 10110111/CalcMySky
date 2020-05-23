#include "ToolsWidget.hpp"
#include <cmath>
constexpr double degree=M_PI/180;

namespace
{

Manipulator* addManipulator(QVBoxLayout*const layout, ToolsWidget*const emitter, void (ToolsWidget::*const signal)(double),
                            QString const& label, const double min, const double max, const double defaultValue,
                            const int decimalPlaces, QString const& unit="")
{
    const auto manipulator=new Manipulator(label, min, max, defaultValue, decimalPlaces);
    layout->addWidget(manipulator);
    emitter->connect(manipulator, &Manipulator::valueChanged, emitter, signal);
    if(!unit.isEmpty())
        manipulator->setUnit(unit);
    return manipulator;
}

}

ToolsWidget::ToolsWidget(const double maxAltitude, QWidget*const parent)
    : QDockWidget(parent)
    , frameRate(new QLabel("N/A"))
{
    setWindowTitle(tr("Tools"));
    const auto mainWidget=new QWidget(this);
    const auto layout=new QVBoxLayout;
    mainWidget->setLayout(layout);
    setWidget(mainWidget);

    addManipulator(layout, this, &ToolsWidget::altitudeChanged, tr("Altitude"), 0, maxAltitude, 50, 2, " m");
    addManipulator(layout, this, &ToolsWidget::exposureLogChanged, tr("log<sub>10</sub>(exposure)"), -5, 10, -4, 2);
    sunElevation=addManipulator(layout, this, &ToolsWidget::sunElevationChanged, tr("Sun elevation"),  -90,  90, 45, 2, QChar(0x00b0));
    sunAzimuth  =addManipulator(layout, this, &ToolsWidget::sunAzimuthChanged,   tr("Sun azimuth"),   -180, 180,  0, 2, QChar(0x00b0));
    {
        const auto grid=new QGridLayout;
        layout->addLayout(grid);
        grid->setColumnStretch(1,1);

        grid->addWidget(new QLabel(tr("Frame rate: ")), 2, 0);
        grid->addWidget(frameRate, 2, 1);
    }

    layout->addStretch();
}

void ToolsWidget::showSunAzimuth(const double azimuth)
{
    QSignalBlocker block(sunAzimuth);
    sunAzimuth->setValue(azimuth/degree);
}

void ToolsWidget::showSunElevation(const double elevation)
{
    QSignalBlocker block(sunElevation);
    sunElevation->setValue(elevation/degree);
}

void ToolsWidget::showFrameRate(const long long frameTimeInUS)
{
    if(frameTimeInUS<=1e6)
        frameRate->setText(tr("%1 FPS").arg(1e6/frameTimeInUS, 0, 'g', 3));
    else
        frameRate->setText(tr("%1 FPS (%2 s/frame)").arg(1e6/frameTimeInUS, 0, 'g', 3)
                                                    .arg(frameTimeInUS/1e6, 0, 'g', 3));
}
