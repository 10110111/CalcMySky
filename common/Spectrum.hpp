#ifndef INCLUDE_ONCE_4E1E8582_D44B_47A5_BAC2_3D8D4B0DE5A4
#define INCLUDE_ONCE_4E1E8582_D44B_47A5_BAC2_3D8D4B0DE5A4

#include <vector>
#include <stdexcept>
#include <QString>
#include "../common/util.hpp"

class Spectrum
{
public:
    class ResamplingError : public Error
    {
        QString message;
    public:
        ResamplingError(QString const& message) : message(message) {}
        QString errorType() const override { return QObject::tr("Resampling error"); }
        QString what() const override { return message; }
    };
public:
    std::vector<double> wavelengths;
    std::vector<double> values;

public:
    double minWL() const { return wavelengths.front(); }
    double maxWL() const { return wavelengths.back(); }
    bool empty() const { return wavelengths.empty(); }
    auto size() const { return wavelengths.size(); }
    void append(const double wl, const double v);
    double value(const double wl) const;
    Spectrum resample(const double wlMin, const double wlMax, const int pointCount) const;
    static Spectrum parseFromCSV(QByteArray const& data, QString const& filename, int firstLineNum);
};

#endif
