#include <cmath>
#include <iostream>
#include <QSignalBlocker>
#include "Manipulator.hpp"

Manipulator::Manipulator(QString const& labelString, const double min, const double max,
                         const double defaultValue, const int decimalPlaces)
    : slider(new QSlider(Qt::Horizontal))
    , spinbox(new QDoubleSpinBox)
    , label(new QLabel(labelString))
    , decimalMultiplier(std::pow(10, decimalPlaces))
{
    const auto hbox=new QHBoxLayout(this);
    hbox->addWidget(label);
    hbox->addWidget(spinbox);
    hbox->addWidget(slider);
    label->setBuddy(spinbox);
    setLayout(hbox);

    hbox->setContentsMargins(0,0,0,0);

    spinbox->setKeyboardTracking(false);
    spinbox->setDecimals(decimalPlaces);
    spinbox->setSingleStep(1/decimalMultiplier);
    setRange(min, max);

    QObject::connect(slider, &QSlider::valueChanged, this, &Manipulator::onSliderValueChanged);
    QObject::connect(spinbox, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &Manipulator::onSpinboxValueChanged);
    // initialize with default value
    setValue(defaultValue);
}

void Manipulator::onSliderValueChanged(const int val)
{
    const double newValue=val/decimalMultiplier;
    {
        QSignalBlocker block(spinbox);
        spinbox->setValue(newValue);
    }
    emit valueChanged(newValue);
}

void Manipulator::onSpinboxValueChanged(const double val)
{
    const int newValue=std::round(val*decimalMultiplier);
    {
        QSignalBlocker block(slider);
        slider->setValue(newValue);
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
    slider->setRange(min*decimalMultiplier, max*decimalMultiplier);
}

void Manipulator::setMax(const double max)
{
    spinbox->setMaximum(max);
    slider->setMaximum(max*decimalMultiplier);
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
