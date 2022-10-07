#ifndef INCLUDE_ONCE_5D7304A4_A588_478B_B21C_55EA5C4F88C0
#define INCLUDE_ONCE_5D7304A4_A588_478B_B21C_55EA5C4F88C0

#include <QProgressBar>
#include <QDockWidget>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include "Manipulator.hpp"
#include "RadiancePlot.hpp"
#include "GLWidget.hpp"
#include "api/ShowMySky/Settings.hpp"

class QCheckBox;

class ToolsWidget : public QDockWidget, public ShowMySky::Settings
{
    Q_OBJECT

    static constexpr double degree=M_PI/180;

    QVBoxLayout* scattererCheckboxes_=new QVBoxLayout;
    QComboBox* ditheringMode_=new QComboBox;
    QComboBox* ditheringMethod_=new QComboBox;
    QComboBox* solarSpectrumMode_=new QComboBox;
    QComboBox* projection_=new QComboBox;
    QComboBox* colorMode_=new QComboBox;
    QDoubleSpinBox* solarSpectrumTemperature_=new QDoubleSpinBox;
    Manipulator* altitude_=nullptr;
    Manipulator* exposure_=nullptr;
    Manipulator* sunElevation_=nullptr;
    Manipulator* sunAzimuth_=nullptr;
    Manipulator* sunAngularRadius_=nullptr;
    Manipulator* moonElevation_=nullptr;
    Manipulator* moonAzimuth_=nullptr;
    Manipulator* earthMoonDistance_=nullptr;
    Manipulator* zoomFactor_=nullptr;
    Manipulator* cameraPitch_=nullptr;
    Manipulator* cameraYaw_=nullptr;
    Manipulator* lightPollutionGroundLuminance_=nullptr;
    QCheckBox* onTheFlySingleScatteringEnabled_=nullptr;
    QCheckBox* onTheFlyPrecompDoubleScatteringEnabled_=nullptr;
    QCheckBox* zeroOrderScatteringEnabled_=nullptr;
    QCheckBox* singleScatteringEnabled_=nullptr;
    QCheckBox* multipleScatteringEnabled_=nullptr;
    QCheckBox* textureFilteringEnabled_=nullptr;
    QCheckBox* usingEclipseShader_=nullptr;
    QCheckBox* pseudoMirrorEnabled_=nullptr;
    QCheckBox* gradualClippingEnabled_=nullptr;
    QCheckBox* glareEnabled_=nullptr;
    QPushButton* showRadiancePlot_=nullptr;
    std::unique_ptr<QWidget> radiancePlotWindow_;
    RadiancePlot* radiancePlot_=nullptr;
    QCheckBox* windowDecorationEnabled_=nullptr;
    QVector<QCheckBox*> scatterers;
public:
    ToolsWidget(QWidget* parent=nullptr);

    double altitude()       override { return altitude_->value(); }
    double sunAzimuth()     override { return degree*sunAzimuth_->value(); }
    double sunZenithAngle() override { return degree*(90-sunElevation_->value()); }
    double sunAngularRadius() override { return degree*sunAngularRadius_->value(); }
    double moonAzimuth()     override { return degree*moonAzimuth_->value(); }
    double moonZenithAngle() override { return degree*(90-moonElevation_->value()); }
    double earthMoonDistance() override { return 1000*earthMoonDistance_->value(); }
    float zoomFactor() const { return zoomFactor_->value(); }
    float cameraYaw() const { return degree*cameraYaw_->value(); }
    float cameraPitch() const { return degree*cameraPitch_->value(); }
    double lightPollutionGroundLuminance() override { return lightPollutionGroundLuminance_->value(); }
    bool onTheFlySingleScatteringEnabled() override { return onTheFlySingleScatteringEnabled_->isChecked(); }
    bool onTheFlyPrecompDoubleScatteringEnabled() override { return onTheFlyPrecompDoubleScatteringEnabled_->isChecked(); }
    bool zeroOrderScatteringEnabled() override { return zeroOrderScatteringEnabled_->isChecked(); }
    bool singleScatteringEnabled() override { return singleScatteringEnabled_->isChecked(); }
    bool multipleScatteringEnabled() override { return multipleScatteringEnabled_->isChecked(); }
    bool textureFilteringEnabled() override { return textureFilteringEnabled_->isChecked(); }
    bool usingEclipseShader() override { return usingEclipseShader_->isChecked(); }
    bool pseudoMirrorEnabled() override { return pseudoMirrorEnabled_->isChecked(); }
    bool gradualClippingEnabled() const { return gradualClippingEnabled_->isChecked(); }
    bool glareEnabled() const { return glareEnabled_->isChecked(); }
    float exposure() const { return std::pow(10., exposure_->value()); }
    GLWidget::DitheringMode ditheringMode() const { return static_cast<GLWidget::DitheringMode>(ditheringMode_->currentIndex()); }
    GLWidget::DitheringMethod ditheringMethod() const { return static_cast<GLWidget::DitheringMethod>(ditheringMethod_->currentIndex()); }

    bool handleSpectralRadiance(ShowMySky::AtmosphereRenderer::SpectralRadiance const& spectrum);
    void setCanGrabRadiance(bool can);
    void setCanSetSolarSpectrum(bool can);
    void setZoomFactor(double zoom);
    void setCameraPitch(double pitch);
    void setCameraYaw(double yaw);
    void setSunAzimuth(double azimuth);
    void setSunZenithAngle(double elevation);
    void updateParameters(AtmosphereParameters const& params);
    void setWindowDecorationEnabled(bool enabled);

private:
    void showRadiancePlot();
    void onSolarSpectrumChanged();

signals:
    void settingChanged();
    void ditheringMethodChanged();
    void setScattererEnabled(QString const& name, bool enable);
    void reloadShadersClicked();
    void setFlatSolarSpectrum();
    void resetSolarSpectrum();
    void setBlackBodySolarSpectrum(double temperature);
    void windowDecorationToggled(bool enabled);
    void projectionChanged(GLWidget::Projection);
    void colorModeChanged(GLWidget::ColorMode);
};

#endif
