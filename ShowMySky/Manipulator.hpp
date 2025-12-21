/*
 * CalcMySky - a simulator of light scattering in planetary atmospheres
 * Copyright © 2025 Ruslan Kabatsayev
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
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

#ifndef INCLUDE_ONCE_66A25B12_D91A_4496_B4D2_0B1BA1B7A168
#define INCLUDE_ONCE_66A25B12_D91A_4496_B4D2_0B1BA1B7A168

#include <QLabel>
#include <QWidget>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QHBoxLayout>

class Manipulator: public QWidget
{
    Q_OBJECT

    QSlider*const slider;
    QDoubleSpinBox*const spinbox;
    QLabel*const label;
    bool nonlinearSlider;

public:
    Manipulator(QString const& label, double min, double max, double defaultValue, int decimalPlaces=0, bool quasiExponentialSlider=false);

    void setValue(double val);
    void setRange(double min, double max);
    void setMax(double max);
    void setLabel(QString const& text);
    void setUnit(QString const& suffix);
    double value() const;
    double minimum() const;
    double maximum() const;

    void onSpinboxValueChanged(double value);
    void onSliderValueChanged(int value);
signals:
    void valueChanged(double);

private:
    double linearToSlider(double linear) const;
    double sliderToLinear(int slider) const;
};

#endif
