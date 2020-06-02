#ifndef INCLUDE_ONCE_5D7304A4_A588_478B_B21C_55EA5C4F88C0
#define INCLUDE_ONCE_5D7304A4_A588_478B_B21C_55EA5C4F88C0

#include <QDockWidget>
#include <QComboBox>
#include <QCheckBox>
#include "Manipulator.hpp"
#include "AtmosphereRenderer.hpp"

class QCheckBox;

class ToolsWidget : public QDockWidget
{
    Q_OBJECT

    static constexpr double degree=M_PI/180;

    QVBoxLayout* scattererCheckboxes_=new QVBoxLayout;
    QComboBox* ditheringMode_=new QComboBox;
    Manipulator* altitude_=nullptr;
    Manipulator* exposure_=nullptr;
    Manipulator* sunElevation_=nullptr;
    Manipulator* sunAzimuth_=nullptr;
    Manipulator* moonElevation_=nullptr;
    Manipulator* moonAzimuth_=nullptr;
    Manipulator* zoomFactor_=nullptr;
    QCheckBox* onTheFlySingleScatteringEnabled_=nullptr;
    QCheckBox* zeroOrderScatteringEnabled_=nullptr;
    QCheckBox* singleScatteringEnabled_=nullptr;
    QCheckBox* multipleScatteringEnabled_=nullptr;
    QCheckBox* textureFilteringEnabled_=nullptr;
    QCheckBox* usingEclipseShader_=nullptr;
    QLabel*const frameRate=new QLabel("N/A");
    QVector<QCheckBox*> scatterers;
public:
    ToolsWidget(double maxAltitude, QWidget* parent=nullptr);

    AtmosphereRenderer::DitheringMode ditheringMode() const { return static_cast<AtmosphereRenderer::DitheringMode>(ditheringMode_->currentIndex()); }
    double altitude()       const { return altitude_->value(); }
    double sunAzimuth()     const { return degree*sunAzimuth_->value(); }
    double sunZenithAngle() const { return degree*(90-sunElevation_->value()); }
    double moonAzimuth()     const { return degree*moonAzimuth_->value(); }
    double moonZenithAngle() const { return degree*(90-moonElevation_->value()); }
    float zoomFactor() const { return zoomFactor_->value(); }
    bool onTheFlySingleScatteringEnabled() const { return onTheFlySingleScatteringEnabled_->isChecked(); }
    bool zeroOrderScatteringEnabled() const { return zeroOrderScatteringEnabled_->isChecked(); }
    bool singleScatteringEnabled() const { return singleScatteringEnabled_->isChecked(); }
    bool multipleScatteringEnabled() const { return multipleScatteringEnabled_->isChecked(); }
    bool textureFilteringEnabled() const { return textureFilteringEnabled_->isChecked(); }
    bool usingEclipseShader() const { return usingEclipseShader_->isChecked(); }
    float exposure() const { return std::pow(10., exposure_->value()); }

    void setSunAzimuth(double azimuth);
    void setSunZenithAngle(double elevation);
    void showFrameRate(long long frameTimeInUS);
    void updateParameters(AtmosphereRenderer::Parameters const& params);
signals:
    void settingChanged();
    void setScattererEnabled(QString const& name, bool enable);
    void reloadShadersClicked();
};

#endif
