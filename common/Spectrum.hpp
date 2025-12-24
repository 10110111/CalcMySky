/*
 * CalcMySky - a simulator of light scattering in planetary atmospheres
 * Copyright © 2025 Ruslan Kabatsayev
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) version 3.
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

#ifndef INCLUDE_ONCE_4E1E8582_D44B_47A5_BAC2_3D8D4B0DE5A4
#define INCLUDE_ONCE_4E1E8582_D44B_47A5_BAC2_3D8D4B0DE5A4

#include <vector>
#include <stdexcept>
#include <QString>
#include "../common/util.hpp"

class Spectrum
{
public:
    class ResamplingError : public ShowMySky::Error
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
