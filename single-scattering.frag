#version 330
#extension GL_ARB_shading_language_420pack : require

#include "const.h.glsl"
#include "densities.h.glsl"
#include "common-functions.h.glsl"
#include "single-scattering.h.glsl"
#include "texture-sampling-functions.h.glsl"

uniform vec4 rayleighScatteringCoefficient; // cross-section * numberDensityAtSeaLevel => m^-1
uniform vec4 mieScatteringCoefficient; // cross-section * numberDensityAtSeaLevel => m^-1
uniform vec4 solarIrradianceAtTOA; // W/m^2/nm

// This function omits phase function and solar irradiance: these are to be applied somewhere in the calling code.
ScatteringSpectra computeSingleScatteringIntegrand(const float cosSunZenithAngle, const float altitude,
                                                   const float dotViewSun, const float cosViewZenithAngle,
                                                   const float dist, const bool viewRayIntersectsGround)
{
    const float r=earthRadius+altitude;
    // Clamping only guards against rounding errors here, we don't try to handle here the case when the
    // endpoint of the view ray intentionally appears in outer space.
    const float altAtDist=clampAltitude(sqrt(sqr(dist)+sqr(r)+2*r*dist*cosViewZenithAngle)-earthRadius);
    const float cosSunZenithAngleAtDist=clampCosine((r*cosSunZenithAngle+dist*dotViewSun)/(earthRadius+altAtDist));

    const vec4 xmittance=transmittance(cosViewZenithAngle, altitude, dist, viewRayIntersectsGround)
                                                    *
                         transmittanceToSun(cosSunZenithAngleAtDist, altAtDist);
    ScatteringSpectra spectra;
    spectra.rayleigh = xmittance*density(altAtDist, DENSITY_REL_RAYLEIGH);
    spectra.mie      = xmittance*density(altAtDist, DENSITY_REL_MIE);

    return spectra;
}

ScatteringSpectra computeSingleScattering(const float cosSunZenithAngle, const float altitude, const float dotViewSun,
                                          const float cosViewZenithAngle, const bool viewRayIntersectsGround)
{
    const float integrInterval=distanceToNearestAtmosphereBoundary(cosViewZenithAngle, altitude,
                                                                   viewRayIntersectsGround);

    // Using trapezoid rule on a uniform grid: f0/2+f1+f2+...+f(N-2)+f(N-1)/2.
    // Initializing with sum of values at endpoints, with weight of 0.5
    const ScatteringSpectra end1=computeSingleScatteringIntegrand(cosSunZenithAngle, altitude, dotViewSun,
                                                                  cosViewZenithAngle, 0, viewRayIntersectsGround);
    const ScatteringSpectra end2=computeSingleScatteringIntegrand(cosSunZenithAngle, altitude, dotViewSun,
                                                                  cosViewZenithAngle, integrInterval,
                                                                  viewRayIntersectsGround);
    ScatteringSpectra spectra=ScatteringSpectra(end1.rayleigh+end2.rayleigh, end1.mie+end2.mie);
    spectra.rayleigh*=0.5;
    spectra.mie*=0.5;

    const float dl=integrInterval/(radialIntegrationPoints-1);
    for(int n=1; n<radialIntegrationPoints-1; ++n)
    {
        const float dist=n*dl;
        const ScatteringSpectra dSpect=computeSingleScatteringIntegrand(cosSunZenithAngle, altitude, dotViewSun,
                                                                        cosViewZenithAngle, dist,
                                                                        viewRayIntersectsGround);
        spectra.rayleigh += dSpect.rayleigh;
        spectra.mie += dSpect.mie;
    }
    spectra.rayleigh *= dl*solarIrradianceAtTOA*rayleighScatteringCoefficient;
    spectra.mie *= dl*solarIrradianceAtTOA*mieScatteringCoefficient;
    return spectra;
}
