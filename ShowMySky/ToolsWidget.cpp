#include "ToolsWidget.hpp"
#include <QFrame>
#include <QPushButton>
#include <cmath>
#include "RadiancePlot.hpp"

namespace
{

const auto text_drawMultipleScattering=QObject::tr("Draw &multiple scattering layer");
const auto text_drawMultipleScattering_plusSomeSingle=QObject::tr("Draw &multiple (and merged single) scattering layer");

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

void triggerStateChanged(QCheckBox* cb)
{
    {
        QSignalBlocker block(cb);
        cb->setChecked(!cb->isChecked());
    }
    cb->setChecked(!cb->isChecked());
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
    sunElevation_ = addManipulator(layout, this, tr("Sun e&levation"),  -90,  90, 45, 3, QChar(0x00b0));
    sunAzimuth_   = addManipulator(layout, this, tr("Sun az&imuth"),   -180, 180,  0, 3, QChar(0x00b0));
    moonElevation_= addManipulator(layout, this, tr("Moon &elevation"),  -90,  90, 41, 3, QChar(0x00b0));
    moonAzimuth_  = addManipulator(layout, this, tr("Moon azim&uth"),   -180, 180,  0, 3, QChar(0x00b0));
    zoomFactor_   = addManipulator(layout, this, tr("&Zoom"), 1, 100, 1, 1);
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
    zeroOrderScatteringEnabled_ = addCheckBox(layout, this, tr("Draw zer&o-order scattering layer"), true);
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
    multipleScatteringEnabled_  = addCheckBox(layout, this, text_drawMultipleScattering, true);
    textureFilteringEnabled_=addCheckBox(layout, this, tr("&Texture filtering"), true);
    onTheFlySingleScatteringEnabled_=addCheckBox(layout, this, tr("Compute single scattering on the &fly"), false);
    onTheFlyPrecompDoubleScatteringEnabled_=addCheckBox(layout, this, tr("Precompute double(-only) scattering on the fly"), true);

    usingEclipseShader_=addCheckBox(layout, this, tr("Use e&clipse-mode shaders"), false);
    connect(usingEclipseShader_, &QCheckBox::stateChanged, this, [this](const int state)
            {
                 const bool eclipseEnabled = state==Qt::Checked;
                 moonElevation_->setEnabled(eclipseEnabled);
                 moonAzimuth_->setEnabled(eclipseEnabled);
                 // TODO: remove this disabling code after we implement zero-order scattering in eclipsed mode
                 if(eclipseEnabled)
                 {
                     zeroOrderScatteringEnabled_->setChecked(false);
                 }
                 zeroOrderScatteringEnabled_->setEnabled(!eclipseEnabled);
            });
    triggerStateChanged(usingEclipseShader_);

    {
        const auto button=new QPushButton(tr("&Reload shaders"));
        layout->addWidget(button);
        connect(button, &QPushButton::clicked, this, &ToolsWidget::reloadShadersClicked);
    }
    {
        showRadiancePlot_=new QPushButton(tr("Show radiance &plot"));
        layout->addWidget(showRadiancePlot_);
        connect(showRadiancePlot_, &QPushButton::clicked, this, &ToolsWidget::showRadiancePlot);
    }
    {
        const auto grid=new QGridLayout;
        layout->addLayout(grid);
        grid->setColumnStretch(1,1);

        grid->addWidget(new QLabel(tr("Frame rate: ")), 2, 0);
        grid->addWidget(frameRate, 2, 1);
    }
    {
        const auto loadProgressLayout=new QVBoxLayout;
        loadProgressWidget_->setLayout(loadProgressLayout);
        layout->addWidget(loadProgressWidget_);
        loadProgressLayout->addWidget(loadProgressLabel_);
        loadProgressLayout->addWidget(loadProgressBar_);
        loadProgressLabel_->setWordWrap(true);
        loadProgressWidget_->hide();
    }

    layout->addStretch();
}

void ToolsWidget::showRadiancePlot()
{
    if(!radiancePlot_)
        radiancePlot_.reset(new RadiancePlot);
    radiancePlot_->show();
}

bool ToolsWidget::handleSpectralRadiance(AtmosphereRenderer::SpectralRadiance const& spectrum)
{
    if(!radiancePlot_ || !radiancePlot_->isVisible()) return false;
    radiancePlot_->setData(spectrum.wavelengths.data(), spectrum.radiances.data(), spectrum.wavelengths.size(),
                           spectrum.azimuth, spectrum.elevation);
    return true;
}

void ToolsWidget::setCanGrabRadiance(const bool can)
{
    showRadiancePlot_->setEnabled(can);
	if(!can)
	{
		showRadiancePlot_->setToolTip(tr("Radiance is not available because some textures in\n"
										 "the current dataset contain only luminance data.\n"
										 "Use --radiance option for calcmysky command to\n"
										 "generate full-spectral textures."));
	}
	else
	{
		showRadiancePlot_->setToolTip("");
	}
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

void ToolsWidget::updateParameters(AtmosphereParameters const& params)
{
    for(QCheckBox*const checkbox : scatterers)
        delete checkbox;
    scatterers.clear();

    multipleScatteringEnabled_->setText(text_drawMultipleScattering);
    for(const auto& scatterer : params.scatterers)
    {
        if(scatterer.phaseFunctionType==PhaseFunctionType::Smooth)
        {
            multipleScatteringEnabled_->setText(text_drawMultipleScattering_plusSomeSingle);
            continue;
        }

        const auto checkbox=new QCheckBox(scatterer.name);
        checkbox->setChecked(true);
        scattererCheckboxes_->addWidget(checkbox);
        connect(checkbox, &QCheckBox::stateChanged, this,
                [this,checkbox,name=scatterer.name](const int state)
                { emit setScattererEnabled(name, state==Qt::Checked); });
        scatterers.push_back(checkbox);
    }
}

void ToolsWidget::onLoadProgress(QString const& currentActivity, const int stepsDone, const int stepsToDo)
{
    loadProgressLabel_->setText(currentActivity);
    loadProgressBar_->setMaximum(stepsToDo);
    loadProgressBar_->setValue(stepsDone);
    loadProgressWidget_->setVisible(stepsToDo!=0);
    loadProgressWidget_->repaint();
}
