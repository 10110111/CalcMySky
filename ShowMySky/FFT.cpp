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

#include "FFT.hpp"
#include "../common/util.hpp"
#include "util.hpp"
#include <utility>
#include <QOpenGLShaderProgram>

FFT::FFT()
{
}

unsigned FFT::roundSizeUpToSupported(const unsigned size)
{
    return roundUpToClosestPowerOfTwo(size);
}

bool FFT::isSupportedSize(const unsigned size)
{
    return isPowerOfTwo(size);
}

auto FFT::generatePasses(const unsigned fftWidth, const unsigned fftHeight, const bool forward,
                         const unsigned inputTex, unsigned pingTex, unsigned pongTex) -> std::vector<Pass>
{
    if(!isSupportedSize(fftWidth) || !isSupportedSize(fftHeight)) return {};

    if(inputTex == pongTex)
        std::swap(pingTex, pongTex);

    const unsigned xIterations = std::lround(std::log2(fftWidth));
    const unsigned yIterations = std::lround(std::log2(fftHeight));
    const unsigned iterations = xIterations + yIterations;

    std::vector<Pass> passes;
    for(unsigned i = 0; i < iterations; i++)
    {
        const bool horizontal = i < xIterations;
        const float subtransformSize = 2 << (horizontal ? i : i - xIterations);
        const float normalization = i == 0 ? 1 / std::sqrt(float(fftWidth * fftHeight)) : 1;
        passes.push_back({
            .inputTex = i == 0 ? inputTex : pingTex,
            .outputTex = pongTex,
            .horizontal = horizontal,
            .forward = forward,
            .resolution = {1.0f / fftWidth, 1.0f / fftHeight},
            .normalization = normalization,
            .subtransformSize = subtransformSize,
        });

        std::swap(pingTex, pongTex);
    }

    return passes;
}

void FFT::init()
{
    shaderProgram_=std::make_unique<QOpenGLShaderProgram>();
    addShaderCode(*shaderProgram_, QOpenGLShader::Vertex, QObject::tr("glare FFT vertex shader"), 1+R"(
#version 330
in vec3 vertex;
void main()
{
    gl_Position=vec4(vertex,1);
}
)");
    addShaderCode(*shaderProgram_, QOpenGLShader::Fragment, QObject::tr("glare FFT fragment shader"), 1+R"(
#version 330

// This implementation is based on Ricky Reusser's glsl-fft code.
vec4 fft(sampler2D src, vec2 resolution, float subtransformSize, bool horizontal,
         bool forward, float normalization, vec2 inputShift, vec2 fftSize)
{
    const float PI=3.1415926535897932;

    float index = (horizontal ? gl_FragCoord.x : gl_FragCoord.y) - 0.5;

    float evenIndex = floor(index / subtransformSize) * (subtransformSize * 0.5) +
                        mod(index, subtransformSize * 0.5) + 0.5;

    vec2 evenPos, oddPos;
    if(horizontal)
    {
        evenPos = vec2(evenIndex, gl_FragCoord.y);
        oddPos = vec2(evenIndex, gl_FragCoord.y);
    }
    else
    {
        evenPos = vec2(gl_FragCoord.x, evenIndex);
        oddPos = vec2(gl_FragCoord.x, evenIndex);
    }

    evenPos *= resolution;
    oddPos *= resolution;

    if(horizontal)
        oddPos.x += 0.5;
    else
        oddPos.y += 0.5;

    // Fixup for a possibly smaller input texture, so that we sample it without
    // stretching, relying on GL_TEXTURE_WRAP_{S,T} behavior for padding.
    vec2 scale = fftSize / textureSize(src,0);
    evenPos *= scale;
    oddPos *= scale;
    if(inputShift.x != 0 || inputShift.y != 0)
    {
        vec2 period = scale;
        evenPos = mod(evenPos + inputShift, scale);
        oddPos = mod(oddPos + inputShift, scale);
    }
    vec4 even = texture(src, evenPos);
    vec4 odd = texture(src, oddPos);

    float twiddleArgument = (forward ? 2*PI : -2*PI) * (index / subtransformSize);
    vec2 twiddle = vec2(cos(twiddleArgument), sin(twiddleArgument));

    return (even.rgba + vec4(
                             twiddle.x * odd.xz - twiddle.y * odd.yw,
                             twiddle.y * odd.xz + twiddle.x * odd.yw
                            ).xzyw) * normalization;
}

uniform sampler2D tex;
uniform vec2 resolution;
uniform float subtransformSize;
uniform bool horizontal;
uniform bool forward;
uniform float normalization;
uniform vec2 inputShift;
uniform vec2 fftSize;

out vec4 color;

void main()
{
    color = fft(tex, resolution, subtransformSize, horizontal, forward, normalization, inputShift, fftSize);
}
)");
    link(*shaderProgram_, QObject::tr("glare FFT shader program"));
}
