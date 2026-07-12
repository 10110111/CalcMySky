/*
 * CalcMySky - a simulator of light scattering in planetary atmospheres
 * Copyright © 2026 Ruslan Kabatsayev
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

#ifndef INCLUDE_ONCE_06766DCE_142F_47F9_91B7_8649F2E670FC
#define INCLUDE_ONCE_06766DCE_142F_47F9_91B7_8649F2E670FC

#include <vector>
#include <memory>

class QOpenGLShaderProgram;
/**
 * This class implements FFT, applying it to 4 channels of the input image in such
 * a way that there are effectively two inputs and two corresponding outputs:
 *  1. r+i*g -> R+i*G,
 *  2. b+i*a -> B+i*A.
 * This lets us implement a convolution of the input RGBA image with a real-valued
 * kernel as if we were doing 4 convolutions at the same time: one per channel.
 */
class FFT
{
public:
    FFT();
    void init();
    static unsigned roundSizeUpToSupported(unsigned size);
    static bool isSupportedSize(unsigned size);

    struct Pass
    {
        unsigned inputTex;
        unsigned outputTex;
        bool horizontal;
        bool forward;
        float resolution[2];
        float fftSize[2];
        float normalization;
        float subtransformSize;
    };
    /**
     * \brief Generate sets of uniforms for each pass of FFT
     * \param fftWidth width of the FFT output
     * \param fftHeight height of the FFT output
     * \param forward whether the FFT is forward or backward (decides on the ± sign of the exponential kernel)
     * \param inputTex the id of the input texture to be read from #Pass::inputTex or #Pass::outputTex
     * \param pingTex the id of the first scratch texture to be read from #Pass::inputTex or #Pass::outputTex
     * \param pongTex the id of the second scratch texture to be read from #Pass::inputTex or #Pass::outputTex
     */
    static std::vector<Pass> generatePasses(unsigned fftWidth, unsigned fftHeight, bool forward,
                                            unsigned inputTex, unsigned pingTex, unsigned pongTex);
    QOpenGLShaderProgram& program() { return *shaderProgram_; }
    bool initialized() const { return !!shaderProgram_; }
private:
    std::unique_ptr<QOpenGLShaderProgram> shaderProgram_;
};

#endif
