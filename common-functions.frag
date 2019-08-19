#version 330
#extension GL_ARB_shading_language_420pack : require

#include "const.h.glsl"

float rayleighScattererRelativeDensity(float altitude);
float mieScattererRelativeDensity(float altitude);
float ozoneDensity(float altitude);

float density(float altitude, int whichDensity)
{
    switch(whichDensity)
    {
    case DENSITY_ABS_OZONE:
        return ozoneDensity(altitude);
    case DENSITY_REL_RAYLEIGH:
        return rayleighScattererRelativeDensity(altitude);
    case DENSITY_REL_MIE:
        return mieScattererRelativeDensity(altitude);
    }
}

float distanceToAtmosphereBorder(float observerAltitude, float cosZenithAngle)
{
    const float Robs=earthRadius+observerAltitude;
    const float Ratm=earthRadius+atmosphereHeight;
    float discriminant=sqr(Ratm)-sqr(Robs)*(1-sqr(cosZenithAngle));
    if(discriminant<0 && abs(Robs/Ratm-1)<1e-4)
        discriminant=0; // This can only happen due to rounding errors, so fix it
    return sqrt(discriminant)-Robs*cosZenithAngle;
}
