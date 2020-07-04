#ifndef INCLUDE_ONCE_3A48838B_2D1A_4326_9585_2E19F9D300D1
#define INCLUDE_ONCE_3A48838B_2D1A_4326_9585_2E19F9D300D1

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
