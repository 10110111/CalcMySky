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
#include "api/Settings.hpp"

class QCheckBox;

class ToolsWidget : public QDockWidget, public ShowMySky::Settings
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
    QCheckBox* onTheFlyPrecompDoubleScatteringEnabled_=nullptr;
    QCheckBox* zeroOrderScatteringEnabled_=nullptr;
    QCheckBox* singleScatteringEnabled_=nullptr;
    QCheckBox* multipleScatteringEnabled_=nullptr;
    QCheckBox* textureFilteringEnabled_=nullptr;
    QCheckBox* usingEclipseShader_=nullptr;
    QCheckBox* gradualClippingEnabled_=nullptr;
    QPushButton* showRadiancePlot_=nullptr;
    std::unique_ptr<RadiancePlot> radiancePlot_;
    QWidget*const loadProgressWidget_=new QWidget;
    QLabel*const frameRate=new QLabel("N/A");
    QLabel*const loadProgressLabel_=new QLabel("Loading...");
    QProgressBar*const loadProgressBar_=new QProgressBar;
    QVector<QCheckBox*> scatterers;
public:
    ToolsWidget(QWidget* parent=nullptr);

    double altitude()       override { return altitude_->value(); }
    double sunAzimuth()     override { return degree*sunAzimuth_->value(); }
    double sunZenithAngle() override { return degree*(90-sunElevation_->value()); }
    double moonAzimuth()     override { return degree*moonAzimuth_->value(); }
    double moonZenithAngle() override { return degree*(90-moonElevation_->value()); }
    float zoomFactor() const { return zoomFactor_->value(); }
    bool onTheFlySingleScatteringEnabled() override { return onTheFlySingleScatteringEnabled_->isChecked(); }
    bool onTheFlyPrecompDoubleScatteringEnabled() override { return onTheFlyPrecompDoubleScatteringEnabled_->isChecked(); }
    bool zeroOrderScatteringEnabled() override { return zeroOrderScatteringEnabled_->isChecked(); }
    bool singleScatteringEnabled() override { return singleScatteringEnabled_->isChecked(); }
    bool multipleScatteringEnabled() override { return multipleScatteringEnabled_->isChecked(); }
    bool textureFilteringEnabled() override { return textureFilteringEnabled_->isChecked(); }
    bool usingEclipseShader() override { return usingEclipseShader_->isChecked(); }
    bool gradualClippingEnabled() const { return gradualClippingEnabled_->isChecked(); }
    float exposure() const { return std::pow(10., exposure_->value()); }
    GLWidget::DitheringMode ditheringMode() const { return static_cast<GLWidget::DitheringMode>(ditheringMode_->currentIndex()); }

    bool handleSpectralRadiance(ShowMySky::AtmosphereRenderer::SpectralRadiance const& spectrum);
    void setCanGrabRadiance(bool can);
    void setZoomFactor(double zoom);
    void setSunAzimuth(double azimuth);
    void setSunZenithAngle(double elevation);
    void showFrameRate(long long frameTimeInUS);
    void updateParameters(AtmosphereParameters const& params);
    void onLoadProgress(QString const& currentActivity, int stepsDone, int stepsToDo);

private:
    void showRadiancePlot();

signals:
    void settingChanged();
    void setScattererEnabled(QString const& name, bool enable);
    void reloadShadersClicked();
};

#endif
