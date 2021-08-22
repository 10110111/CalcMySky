#include <cmath>
#include <cassert>
#include <iostream>
#include <QSignalBlocker>
#include "Manipulator.hpp"

Manipulator::Manipulator(QString const& labelString, const double min, const double max,
                         const double defaultValue, const int decimalPlaces, const bool quasiExponentialSlider)
    : slider(new QSlider(Qt::Horizontal))
    , spinbox(new QDoubleSpinBox)
    , label(new QLabel(labelString))
    , nonlinearSlider(quasiExponentialSlider)
{
    if(nonlinearSlider && (min<0 || max<0))
        assert(!"Quasi-exponential slider can't handle negative values");

    const auto hbox=new QHBoxLayout(this);
    hbox->addWidget(label);
    hbox->addWidget(spinbox);
    hbox->addWidget(slider);
    label->setBuddy(spinbox);
    setLayout(hbox);

    hbox->setContentsMargins(0,0,0,0);

    spinbox->setKeyboardTracking(false);
    spinbox->setDecimals(decimalPlaces);
    spinbox->setSingleStep(std::pow(10, -decimalPlaces));
    setRange(min, max);

    QObject::connect(slider, &QSlider::valueChanged, this, &Manipulator::onSliderValueChanged);
    QObject::connect(spinbox, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &Manipulator::onSpinboxValueChanged);
    // initialize with default value
    setValue(defaultValue);
}

double Manipulator::linearToSlider(const double linear) const
{
    return nonlinearSlider ? std::log10(linear - minimum() + 1)/std::log10(maximum() - minimum() + 1)*INT_MAX
                           : (linear - minimum())/(maximum() - minimum())*INT_MAX;
}
double Manipulator::sliderToLinear(const int slider) const
{
    return nonlinearSlider ? std::pow(10., double(slider)/INT_MAX*std::log10(maximum() - minimum() + 1)) + minimum() - 1
                           : double(slider)/INT_MAX*(maximum() - minimum()) + minimum();
}

void Manipulator::onSliderValueChanged(const int val)
{
    const double newValue = sliderToLinear(val);
    {
        QSignalBlocker block(spinbox);
        spinbox->setValue(newValue);
    }
    emit valueChanged(spinbox->value());
}

void Manipulator::onSpinboxValueChanged(const double val)
{
    {
        QSignalBlocker block(slider);
        slider->setValue(linearToSlider(val));
    }
    emit valueChanged(val);
}

double Manipulator::value() const
{
    return spinbox->value();
}

void Manipulator::setValue(const double val)
{
    spinbox->setValue(val);
}

void Manipulator::setRange(const double min, const double max)
{
    spinbox->setRange(min, max);
    slider->setRange(linearToSlider(min), linearToSlider(max));
    // Move the slider handle to its new location
    {
        QSignalBlocker block(slider);
        slider->setValue(linearToSlider(value()));
    }
}

void Manipulator::setMax(const double max)
{
    setRange(minimum(), max);
}

void Manipulator::setLabel(QString const& text)
{
    label->setText(text);
}

void Manipulator::setUnit(QString const& suffix)
{
    spinbox->setSuffix(suffix);
}

double Manipulator::minimum() const
{
    return spinbox->minimum();
}

double Manipulator::maximum() const
{
    return spinbox->maximum();
}
