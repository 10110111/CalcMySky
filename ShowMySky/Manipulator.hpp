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
    const double decimalMultiplier;

public:
    Manipulator(QString const& label, double min, double max, double defaultValue, int decimalPlaces=0);

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
};

#endif
