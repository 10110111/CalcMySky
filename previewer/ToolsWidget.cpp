#include "ToolsWidget.hpp"
#include <QFrame>
#include <QPushButton>
#include <cmath>

namespace
{

Manipulator* addManipulator(QVBoxLayout*const layout, ToolsWidget*const tools,
                            QString const& label, const double min, const double max, const double defaultValue,
                            const int decimalPlaces, QString const& unit="")
{
    const auto manipulator=new Manipulator(label, min, max, defaultValue, decimalPlaces);
    layout->addWidget(manipulator);
    tools->connect(manipulator, &Manipulator::valueChanged, tools, &ToolsWidget::settingChanged);
    if(!unit.isEmpty())
        manipulator->setUnit(unit);
    return manipulator;
}

QCheckBox* addCheckBox(QVBoxLayout*const layout, ToolsWidget*const tools,
                       QString const& label, const bool initState)
{
    const auto checkbox=new QCheckBox(label);
    checkbox->setChecked(initState);
    layout->addWidget(checkbox);
    tools->connect(checkbox, &QCheckBox::stateChanged, tools, &ToolsWidget::settingChanged);
    return checkbox;
}

}

ToolsWidget::ToolsWidget(const double maxAltitude, QWidget*const parent)
    : QDockWidget(parent)
{
    setWindowTitle(tr("Tools"));
    const auto mainWidget=new QWidget(this);
    const auto layout=new QVBoxLayout;
    mainWidget->setLayout(layout);
    setWidget(mainWidget);

    altitude_     = addManipulator(layout, this, tr("&Altitude"), 0, maxAltitude, 50, 2, " m");
    exposure_     = addManipulator(layout, this, tr("log<sub>10</sub>(e&xposure)"), -5, 3, -4.2, 2);
    sunElevation_ = addManipulator(layout, this, tr("Sun e&levation"),  -90,  90, 45, 2, QChar(0x00b0));
    sunAzimuth_   = addManipulator(layout, this, tr("Sun az&imuth"),   -180, 180,  0, 2, QChar(0x00b0));
    {
        ditheringMode_->addItem(tr("Disabled"));
        ditheringMode_->addItem(tr("5/6/5-bit"));
        ditheringMode_->addItem(tr("6/6/6-bit"));
        ditheringMode_->addItem(tr("8/8/8-bit"));
        ditheringMode_->addItem(tr("10/10/10-bit"));
        ditheringMode_->setCurrentIndex(static_cast<int>(AtmosphereRenderer::DitheringMode::Color888));
        connect(ditheringMode_, qOverload<int>(&QComboBox::currentIndexChanged), this, &ToolsWidget::settingChanged);
        const auto hbox=new QHBoxLayout;
        const auto label=new QLabel(tr("&Dithering"));
        label->setBuddy(ditheringMode_);
        hbox->addWidget(label);
        hbox->addWidget(ditheringMode_);
        ditheringMode_->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
        layout->addLayout(hbox);
    }
    zeroOrderScatteringEnabled_ = addCheckBox(layout, this, tr("Draw &zero-order scattering layer"), true);
    singleScatteringEnabled_    = addCheckBox(layout, this, tr("Draw &single scattering layers"), true);
    {
        const auto frame=new QFrame;
        frame->setLayout(scattererCheckboxes_);
        auto margins=frame->contentsMargins();
        margins.setLeft(singleScatteringEnabled_->style()->pixelMetric(QStyle::PM_IndicatorWidth));
        frame->setContentsMargins(margins);
        layout->addWidget(frame);
        connect(singleScatteringEnabled_, &QCheckBox::stateChanged, frame, [frame](const int state)
                { frame->setEnabled(state==Qt::Checked); });
    }
    multipleScatteringEnabled_  = addCheckBox(layout, this, tr("Draw &multiple scattering layer"), true);
    onTheFlySingleScatteringEnabled_=addCheckBox(layout, this, tr("Compute single scattering on the &fly"), false);
    {
        const auto button=new QPushButton(tr("&Reload shaders"));
        layout->addWidget(button);
        connect(button, &QPushButton::clicked, this, &ToolsWidget::reloadShadersClicked);
    }
    {
        const auto grid=new QGridLayout;
        layout->addLayout(grid);
        grid->setColumnStretch(1,1);

        grid->addWidget(new QLabel(tr("Frame rate: ")), 2, 0);
        grid->addWidget(frameRate, 2, 1);
    }

    layout->addStretch();
}

void ToolsWidget::setSunAzimuth(const double azimuth)
{
    QSignalBlocker block(sunAzimuth_);
    sunAzimuth_->setValue(azimuth/degree);
}

void ToolsWidget::setSunZenithAngle(const double zenithAngle)
{
    QSignalBlocker block(sunElevation_);
    sunElevation_->setValue(90-zenithAngle/degree);
}

void ToolsWidget::showFrameRate(const long long frameTimeInUS)
{
    if(frameTimeInUS<=1e6)
        frameRate->setText(tr("%1 FPS").arg(1e6/frameTimeInUS, 0, 'g', 3));
    else
        frameRate->setText(tr("%1 FPS (%2 s/frame)").arg(1e6/frameTimeInUS, 0, 'g', 3)
                                                    .arg(frameTimeInUS/1e6, 0, 'g', 3));
}

void ToolsWidget::updateParameters(AtmosphereRenderer::Parameters const& params)
{
    for(QCheckBox*const checkbox : scatterers)
        delete checkbox;
    scatterers.clear();

    for(const auto& [scattererName,phaseFunctionType] : params.scatterers)
    {
        if(phaseFunctionType==PhaseFunctionType::Smooth)
            continue;

        const auto checkbox=new QCheckBox(scattererName);
        checkbox->setChecked(true);
        scattererCheckboxes_->addWidget(checkbox);
        connect(checkbox, &QCheckBox::stateChanged, this,
                [this,checkbox,scattererName](const int state)
                { emit setScattererEnabled(scattererName, state==Qt::Checked); });
        scatterers.push_back(checkbox);
    }
}
