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

#ifndef INCLUDE_ONCE_3A48838B_2D1A_4326_9585_2E19F9D300D1
#define INCLUDE_ONCE_3A48838B_2D1A_4326_9585_2E19F9D300D1

#include <cassert>
#include <algorithm>
#include <unsupported/Eigen/FFT>

void fourierInterpolate(float const*const points, const std::size_t inPointCount,
                        std::complex<float>*const intermediate /* must fit interpolationPointCount elements */,
                        float*const interpolated, std::size_t const interpolationPointCount)
{
    if(inPointCount==interpolationPointCount)
    {
        std::copy_n(points, inPointCount, interpolated);
        return;
    }

    assert(interpolationPointCount > inPointCount);

    Eigen::FFT<float> fft;
    fft.fwd(intermediate, points, inPointCount);
    if(inPointCount % 2)
    {
        const auto fftHalfCount=(inPointCount+1)/2;
        // Clear upper half of the spectrum. Since Eigen inverse FFT ignores upper half when output
        // type is real, don't bother preserving it in extended spectrum.
        std::fill_n(intermediate+fftHalfCount, interpolationPointCount-fftHalfCount, 0);
    }
    else
    {
        const auto fftHalfCount=inPointCount/2;
        // Nyquist frequency gets split into two half-amplitude components in the two sides of the spectrum.
        // So we need to 1) avoid zeroing this spectral component out, and 2) divide both its instances by 2.
        // In practice, since Eigen ignores upper half of the spectrum when output type of inverse transform
        // is real, we don't bother saving/moving/dividing the upper entries, and just zero them out too.
        // So only the lower instance of Nyquist frequency amplitude remains to be divided.
        const auto numPreservedElems=fftHalfCount+1;
        std::fill_n(intermediate+numPreservedElems, interpolationPointCount-numPreservedElems, 0);
        intermediate[fftHalfCount] /= 2;
    }
    fft.inv(interpolated, intermediate, interpolationPointCount);
    for(std::size_t i=0; i<interpolationPointCount; ++i)
        interpolated[i] *= float(interpolationPointCount)/inPointCount;
}

#endif
