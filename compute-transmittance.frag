#version 330
#extension GL_ARB_shading_language_420pack : require

#include "const.h.glsl"

in vec3 position;
out vec4 color;

uniform vec4 ozoneAbsorptionCrossSection; // m^2
uniform vec4 rayleighScatteringCoefficient; // cross-section * numberDensityAtSeaLevel => m^-1
uniform vec4 mieScatteringCoefficient; // cross-section * numberDensityAtSeaLevel => m^-1

#include "densities.h.glsl"
#include "texture-coordinates.h.glsl"
#include "common-functions.h.glsl"

vec4 opticalDepthToAtmosphereBorder(float altitude, float cosZenithAngle,
                                    int whichDensity, vec4 crossSection)
{
    const float integrInterval=distanceToAtmosphereBorder(cosZenithAngle, altitude);

    const float R=earthRadius;
    const float r1=R+altitude;
    const float l=integrInterval;
    const float mu=cosZenithAngle;
    // From law of cosines: r₂²=r₁²+l²+2r₁lμ
    const float endAltitude=-R+sqrt(sqr(r1)+sqr(l)+2*r1*l*mu);

    const float dl=integrInterval/(numTransmittanceIntegrationPoints-1);

    // Using trapezoid rule on a uniform grid: f0/2+f1+f2+...+f(N-2)+f(N-1)/2.
    float sum=(density(altitude,whichDensity)+density(endAltitude,whichDensity))/2;
    for(int n=1;n<numTransmittanceIntegrationPoints-1;++n)
    {
        const float dist=n*dl;
        const float currAlt=-R+sqrt(sqr(r1)+sqr(dist)+2*r1*dist*mu);
        sum+=density(currAlt,whichDensity);
    }
    return sum*dl*crossSection;
}

// This assumes that ray doesn't intersect Earth
vec4 computeTransmittanceToAtmosphereBorder(float cosZenithAngle, float altitude)
{
    return exp(-(opticalDepthToAtmosphereBorder(altitude,cosZenithAngle,DENSITY_REL_RAYLEIGH,
                                                rayleighScatteringCoefficient)
                                                            +
                 opticalDepthToAtmosphereBorder(altitude,cosZenithAngle,DENSITY_REL_MIE,
                                                mieScatteringCoefficient/mieSingleScatteringAlbedo)
                                                            +
                 opticalDepthToAtmosphereBorder(altitude,cosZenithAngle,DENSITY_ABS_OZONE,
                                                ozoneAbsorptionCrossSection)));
}

void main()
{
    const vec2 texCoord=0.5*position.xy+vec2(0.5);
    const TransmittanceTexVars vars=transmittanceTexCoordToTexVars(texCoord);
    color=computeTransmittanceToAtmosphereBorder(vars.cosViewZenithAngle, vars.altitude);
}
