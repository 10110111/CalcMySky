#version 330
#include "version.h.glsl"
#include "const.h.glsl"
#include "airglow.h.glsl"
#include "common-functions.h.glsl"
#include "texture-coordinates.h.glsl"
#include "texture-sampling-functions.h.glsl"

layout(location=0) out vec4 scatteringTextureOutput;

// Get transmittance, but prepare the parameters for passing to transmittance() so they are in the correct range
vec4 transmittanceExt(float cosViewZenithAngle, float altitude, float dist, const float altAtDist, const bool viewRayIntersectsGround)
{
    if(altitude > atmosphereHeight)
    {
        vec3 cameraPosition = vec3(0,0, altitude);
        CONST vec3 p = cameraPosition - earthCenter;
        CONST vec3 viewDir = vec3(0, safeSqrt(1-sqr(cosViewZenithAngle)), cosViewZenithAngle);
        CONST float p_dot_v = dot(p, viewDir);
        CONST float p_dot_p = dot(p, p);
        CONST float squaredDistBetweenViewRayAndEarthCenter = p_dot_p - sqr(p_dot_v);
        CONST float distanceToTOA = -p_dot_v - sqrt(sqr(earthRadius+atmosphereHeight) - squaredDistBetweenViewRayAndEarthCenter);
        if(distanceToTOA>=0)
        {
            cameraPosition += viewDir*distanceToTOA;
            altitude = atmosphereHeight;
            CONST vec3 zenith = normalize(cameraPosition-earthCenter);
            cosViewZenithAngle = dot(zenith,viewDir);
            if(altAtDist > atmosphereHeight)
            {
                dist = distanceToNearestAtmosphereBoundary(cosViewZenithAngle, altitude, viewRayIntersectsGround);
            }
            else
            {
                dist -= distanceToTOA;
                if(dist < 0) return vec4(1); // not reaching the extincting part of the atmosphere
            }
        }
        else
        {
            // Looking above the atmosphere managed by the transmittance texture, consider this space fully transparent
            return vec4(1);
        }
    }

    return transmittance(cosViewZenithAngle, altitude, dist, viewRayIntersectsGround);
}

vec4 computeAirglow(const float cosViewZenithAngle, const float altitude, const bool viewRayIntersectsGround)
{
    CONST float r=earthRadius+altitude;
    CONST float integrInterval=distanceToNearestAtmosphereBoundaryForAirglow(cosViewZenithAngle, altitude,
                                                                             viewRayIntersectsGround);
    // Using the midpoint rule for quadrature
    vec4 spectrum=vec4(0);
    CONST float dl=integrInterval/radialIntegrationPointsForAirglow;
    for(int n=0; n<radialIntegrationPointsForAirglow; ++n)
    {
        CONST float dist=(n+0.5)*dl;
        CONST float altAtDist=clampAltitudeForAirglow(sqrt(sqr(dist)+sqr(r)+2*r*dist*cosViewZenithAngle)-earthRadius);
        CONST vec4 xmittance = transmittanceExt(cosViewZenithAngle, altitude, dist, altAtDist, viewRayIntersectsGround);
        spectrum += airglowProfile(altAtDist) * xmittance;
    }
    return spectrum*dl;
}

void main()
{
    CONST AirglowTexVars vars=scatteringTexIndicesToAirglowTexVars(gl_FragCoord.xy-vec2(0.5));
    scatteringTextureOutput=computeAirglow(vars.cosViewZenithAngle, vars.altitude, vars.viewRayIntersectsGround);
}
