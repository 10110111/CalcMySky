#ifndef INCLUDE_ONCE_5D7304A4_A588_478B_B21C_55EA5C4F88C0
#define INCLUDE_ONCE_5D7304A4_A588_478B_B21C_55EA5C4F88C0

#include <QDockWidget>
#include "Manipulator.hpp"

class ToolsWidget : public QDockWidget
{
    Q_OBJECT

    Manipulator* sunElevation;
    Manipulator* sunAzimuth;
    QLabel*const frameRate;
public:
    ToolsWidget(double maxAltitude, QWidget* parent=nullptr);

    void showSunAzimuth(double azimuth);
    void showSunElevation(double elevation);
    void showFrameRate(long long frameTimeInUS);
signals:
    void altitudeChanged(double altitude);
    void exposureLogChanged(double exposureLog);
    void sunElevationChanged(double elevationDeg);
    void sunAzimuthChanged(double azimuthDeg);
};

#endif
