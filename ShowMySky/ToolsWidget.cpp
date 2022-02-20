#include "ToolsWidget.hpp"
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <cmath>
#include "RadiancePlot.hpp"
#include "DockScrollArea.hpp"

namespace
{

const auto text_drawMultipleScattering=QObject::tr("Draw &multiple scattering layer");
const auto text_drawMultipleScattering_plusSomeSingle=QObject::tr("Draw &multiple (and merged single) scattering layer");
constexpr double initialMaxAltitude = 1e9;

enum class SolarSpectrumMode
{
    Precomputed,
    BlackBody,
    Flat,
};
const std::map<SolarSpectrumMode, QString> solarSpectrumModes={
    {SolarSpectrumMode::Precomputed, QObject::tr("Precomputed (default)")},
    {SolarSpectrumMode::BlackBody,   QObject::tr("Black body")},
    {SolarSpectrumMode::Flat,        QObject::tr(u8"Flat 1\u202fW/m\u00b2/nm")},
};

Manipulator* addManipulator(QVBoxLayout*const layout, ToolsWidget*const tools,
                            QString const& label, const double min, const double max, const double defaultValue,
                            const int decimalPlaces, QString const& unit="", const bool nonlinearSlider=false)
{
    const auto manipulator=new Manipulator(label, min, max, defaultValue, decimalPlaces, nonlinearSlider);
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

ToolsWidget::ToolsWidget(QWidget*const parent)
    : QDockWidget(parent)
{
    setWindowTitle(tr("Tools"));
    const auto mainWidget=new QWidget;
    const auto scrollArea=new DockScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameStyle(QFrame::Plain);
    scrollArea->setWidget(mainWidget);
    const auto layout=new QVBoxLayout;
    mainWidget->setLayout(layout);
    setWidget(scrollArea);

    altitude_     = addManipulator(layout, this, tr("&Altitude"), 0, initialMaxAltitude, 50, 2, " m", true);
    exposure_     = addManipulator(layout, this, tr("log<sub>10</sub>(e&xposure)"), -5, 3, -4.2, 2);
    sunElevation_ = addManipulator(layout, this, tr("Sun e&levation"),  -90,  90, 45, 3, QChar(0x00b0));
    sunAzimuth_   = addManipulator(layout, this, tr("Sun az&imuth"),   -180, 180,  0, 3, QChar(0x00b0));
    sunAngularRadius_ = addManipulator(layout, this, tr("Sun angular radius"), 0.01, 0.999,  0.25, 3, QChar(0x00b0));
    moonElevation_= addManipulator(layout, this, tr("Moon &elevation"),  -90,  90, 41, 3, QChar(0x00b0));
    moonAzimuth_  = addManipulator(layout, this, tr("Moon azim&uth"),   -180, 180,  0, 3, QChar(0x00b0));
    earthMoonDistance_ = addManipulator(layout, this, tr("Earth-Moon distance"), 300e3, 1e6,  371925, 0, u8"\u202fkm");
    zoomFactor_   = addManipulator(layout, this, tr("&Zoom"), 1, 1e4, 1, 1, "", true);
    cameraPitch_  = addManipulator(layout, this, tr("Camera pitch"), -90, 90, 0, 2, QChar(0x00b0));
    cameraYaw_    = addManipulator(layout, this, tr("Camera yaw")  ,-180,180, 0, 2, QChar(0x00b0));
    {
        ditheringMethod_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        ditheringMethod_->addItem(tr("No dithering"));
        ditheringMethod_->addItem(tr("Ordered dithering (Bayer)"));
        ditheringMethod_->addItem(tr("Optimized blue noise with triangular remapping"));
        ditheringMethod_->setCurrentIndex(static_cast<int>(GLWidget::DitheringMethod::BlueNoiseTriangleRemapped));
        connect(ditheringMethod_, qOverload<int>(&QComboBox::currentIndexChanged), this, &ToolsWidget::ditheringMethodChanged);
        const auto hbox=new QHBoxLayout;
        const auto label=new QLabel(tr("Ditheri&ng method"));
        label->setBuddy(ditheringMethod_);
        hbox->addWidget(label);
        hbox->addWidget(ditheringMethod_);
        ditheringMethod_->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
        layout->addLayout(hbox);
    }
    {
        ditheringMode_->addItem(tr("5/6/5-bit"));
        ditheringMode_->addItem(tr("6/6/6-bit"));
        ditheringMode_->addItem(tr("8/8/8-bit"));
        ditheringMode_->addItem(tr("10/10/10-bit"));
        ditheringMode_->setCurrentIndex(static_cast<int>(GLWidget::DitheringMode::Color888));
        connect(ditheringMode_, qOverload<int>(&QComboBox::currentIndexChanged), this, &ToolsWidget::settingChanged);
        const auto hbox=new QHBoxLayout;
        const auto label=new QLabel(tr("&Dithering color depth"));
        label->setBuddy(ditheringMode_);
        hbox->addWidget(label);
        hbox->addWidget(ditheringMode_);
        ditheringMode_->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
        layout->addLayout(hbox);
    }
    gradualClippingEnabled_ = addCheckBox(layout, this, tr("&Gradual color clipping"), true);
    glareEnabled_ = addCheckBox(layout, this, tr("Glare (visual only)"), false);
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
    lightPollutionGroundLuminance_ = addManipulator(layout, this, tr("Lig&ht pollution luminance"), 0, 100, 20, 2, QString::fromUtf8(u8"\u202fcd/m\u00b2"));

    {
        const auto hbox=new QHBoxLayout;
        layout->addLayout(hbox);
        const auto label=new QLabel(tr("Solar spectrum"));
        label->setBuddy(solarSpectrumMode_);
        hbox->addWidget(label);
        for(const auto& mode : solarSpectrumModes)
            solarSpectrumMode_->addItem(mode.second, static_cast<int>(mode.first));
        solarSpectrumMode_->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
        connect(solarSpectrumMode_, qOverload<int>(&QComboBox::currentIndexChanged), this, &ToolsWidget::onSolarSpectrumChanged);
        hbox->addWidget(solarSpectrumMode_);
        solarSpectrumTemperature_->setSuffix(QString::fromUtf8(u8"\u202fK"));
        solarSpectrumTemperature_->setRange(1000, 9999);
        solarSpectrumTemperature_->setDecimals(0);
        solarSpectrumTemperature_->setValue(5778);
        solarSpectrumTemperature_->setKeyboardTracking(false);
        auto policy=solarSpectrumTemperature_->sizePolicy();
        policy.setRetainSizeWhenHidden(true);
        solarSpectrumTemperature_->setSizePolicy(policy);
        hbox->addWidget(solarSpectrumTemperature_);
        solarSpectrumTemperature_->hide();
        connect(solarSpectrumTemperature_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &ToolsWidget::onSolarSpectrumChanged);
    }

    textureFilteringEnabled_=addCheckBox(layout, this, tr("&Texture filtering"), true);
    onTheFlySingleScatteringEnabled_=addCheckBox(layout, this, tr("Compute single scattering on the &fly"), false);
    onTheFlyPrecompDoubleScatteringEnabled_=addCheckBox(layout, this, tr("Precompute double(-only) scattering on the fly"), true);

    usingEclipseShader_=addCheckBox(layout, this, tr("Use e&clipse-mode shaders"), false);
    connect(usingEclipseShader_, &QCheckBox::stateChanged, this, [this](const int state)
            {
                 const bool eclipseEnabled = state==Qt::Checked;
                 moonElevation_->setEnabled(eclipseEnabled);
                 moonAzimuth_->setEnabled(eclipseEnabled);
                 earthMoonDistance_->setEnabled(eclipseEnabled);
            });
    triggerStateChanged(usingEclipseShader_);
    pseudoMirrorEnabled_=addCheckBox(layout, this, tr("Pseudo-mirror sky in the ground"), false);

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

    layout->addStretch();
}

void ToolsWidget::showRadiancePlot()
{
    if(!radiancePlotWindow_)
    {
        radiancePlotWindow_.reset(new QWidget);
        radiancePlotWindow_->setWindowTitle(tr("Spectral radiance - ShowMySky"));
        const auto layout=new QVBoxLayout(radiancePlotWindow_.get());
        const auto statusBar=new QLabel;
        statusBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
        statusBar->setFocusPolicy(Qt::NoFocus);
        radiancePlot_=new RadiancePlot(statusBar);
        radiancePlot_->setFocusPolicy(Qt::StrongFocus);
        layout->addWidget(radiancePlot_);
        layout->addWidget(statusBar);
        layout->setContentsMargins(0,0,0,0);
        layout->setSpacing(3);

        const auto width=qApp->primaryScreen()->size().width()/3;
        const auto height=width*0.8;
        radiancePlotWindow_->resize(width,height);
    }
    radiancePlotWindow_->show();
    radiancePlotWindow_->raise();
}

bool ToolsWidget::handleSpectralRadiance(ShowMySky::AtmosphereRenderer::SpectralRadiance const& spectrum)
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

void ToolsWidget::setCanSetSolarSpectrum(const bool can)
{
    solarSpectrumMode_->setEnabled(can);
	if(!can)
	{
        solarSpectrumMode_->setCurrentIndex(0);
		solarSpectrumMode_->setToolTip(tr("Solar spectrum setting is not available because some textures\n"
										 "in the current dataset contain only luminance data.\n"
										 "Use --radiance option for calcmysky command to\n"
										 "generate full-spectral textures."));
	}
	else
	{
		solarSpectrumMode_->setToolTip("");
	}
}

void ToolsWidget::setZoomFactor(const double zoom)
{
    zoomFactor_->setValue(zoom);
}

void ToolsWidget::setCameraPitch(const double pitch)
{
    QSignalBlocker block(cameraPitch_);
    cameraPitch_->setValue(pitch/degree);
}

void ToolsWidget::setCameraYaw(const double yaw)
{
    QSignalBlocker block(cameraYaw_);
    cameraYaw_->setValue(yaw/degree);
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

void ToolsWidget::updateParameters(AtmosphereParameters const& params)
{
    if(params.atmosphereHeight > initialMaxAltitude)
        altitude_->setMax(params.atmosphereHeight);

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

    if(params.noEclipsedDoubleScatteringTextures)
    {
        onTheFlyPrecompDoubleScatteringEnabled_->setChecked(true);
        onTheFlyPrecompDoubleScatteringEnabled_->setDisabled(true);
    }

    sunAngularRadius_->setValue(params.sunAngularRadius / degree);
}

void ToolsWidget::onSolarSpectrumChanged()
{
    const auto newMode = static_cast<SolarSpectrumMode>(solarSpectrumMode_->currentData().toInt());
    if(newMode==SolarSpectrumMode::BlackBody)
        solarSpectrumTemperature_->show();
    else
        solarSpectrumTemperature_->hide();

    switch(newMode)
    {
    case SolarSpectrumMode::Precomputed:
        emit resetSolarSpectrum();
        break;
    case SolarSpectrumMode::BlackBody:
        emit setBlackBodySolarSpectrum(solarSpectrumTemperature_->value());
        break;
    case SolarSpectrumMode::Flat:
        emit setFlatSolarSpectrum();
        break;
    }
}
