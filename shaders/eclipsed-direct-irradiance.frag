#version 330
#include "version.h.glsl"
#include "const.h.glsl"
#include "common-functions.h.glsl"
#include "direct-irradiance.h.glsl"
#include "texture-sampling-functions.h.glsl"

vec4 calcEclipsedDirectGroundIrradiance(const vec3 pointOnGround, const vec3 sunDir, const vec3 moonPos)
{
    CONST float altitude=0; // we are on the ground, after all
    CONST vec3 zenith=normalize(pointOnGround-earthCenter);
    CONST float cosSunZenithAngle=dot(sunDir,zenith);

    CONST float visibility=sunVisibilityDueToMoon(pointOnGround, sunDir, moonPos)
                                                    *
                      // FIXME: this ignores orientation of the crescent of eclipsed Sun WRT horizon
                                    sunVisibility(cosSunZenithAngle, altitude);

    return visibility * computeDirectGroundIrradiance(cosSunZenithAngle, altitude);
}

