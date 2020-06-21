#version 330
#extension GL_ARB_shading_language_420pack : require

#include "const.h.glsl"
#include "texture-coordinates.h.glsl"
#include "radiance-to-luminance.h.glsl"
#include "single-scattering-eclipsed.h.glsl"

in vec3 position;
out vec4 scatteringTextureOutput;

uniform float altitude;
uniform float sunZenithAngle;
uniform vec3 moonPositionRelativeToSunAzimuth;

vec4 solarRadiance()
{
    return solarIrradianceAtTOA/(PI*sqr(sunAngularRadius));
}

void main()
{
    const EclipseScatteringTexVars texVars=eclipseTexCoordsToTexVars(gl_FragCoord.xy/eclipsedSingleScatteringTextureSize, altitude);
    const float sinViewZenithAngle=sqrt(1-sqr(texVars.cosViewZenithAngle));
    const vec3 viewDir=vec3(cos(texVars.azimuthRelativeToSun)*sinViewZenithAngle,
                            sin(texVars.azimuthRelativeToSun)*sinViewZenithAngle,
                            texVars.cosViewZenithAngle);
    const vec3 cameraPosition=vec3(0,0,altitude);
    const vec3 sunDirection=vec3(sin(sunZenithAngle), 0, cos(sunZenithAngle));
    const vec4 radiance=computeSingleScatteringEclipsed(cameraPosition,viewDir,sunDirection,moonPositionRelativeToSunAzimuth,
                                                        texVars.viewRayIntersectsGround);
#if COMPUTE_RADIANCE
    scatteringTextureOutput=radiance;
#elif COMPUTE_LUMINANCE
    scatteringTextureOutput=radianceToLuminance*radiance;
#else
#error What to compute?
#endif
}
