#version 330
#extension GL_ARB_shading_language_420pack : require

#include "const.h.glsl"
#include "calc-view-dir.h.glsl"
#include_if(RENDERING_ANY_SINGLE_SCATTERING) "phase-functions.h.glsl"
#include_if(RENDERING_ANY_ZERO_SCATTERING) "common-functions.h.glsl"
#include_if(RENDERING_ANY_NORMAL_SINGLE_SCATTERING) "single-scattering.h.glsl"
#include_if(RENDERING_ANY_ECLIPSED_SINGLE_SCATTERING) "single-scattering-eclipsed.h.glsl"
#include "texture-coordinates.h.glsl"
#include "radiance-to-luminance.h.glsl"
#include_if(RENDERING_ANY_ZERO_SCATTERING) "texture-sampling-functions.h.glsl"
#include_if(RENDERING_ECLIPSED_ZERO_SCATTERING) "eclipsed-direct-irradiance.h.glsl"
#include_if(RENDERING_ANY_LIGHT_POLLUTION) "texture-sampling-functions.h.glsl"

uniform sampler3D scatteringTexture;
uniform sampler2D eclipsedScatteringTexture;
uniform sampler3D eclipsedDoubleScatteringTextureLower;
uniform sampler3D eclipsedDoubleScatteringTextureUpper;
uniform vec3 cameraPosition;
uniform vec3 sunDirection;
uniform vec3 moonPosition;
uniform float lightPollutionGroundLuminance;
uniform vec4 solarIrradianceFixup=vec4(1); // Used when we want to alter solar irradiance post-precomputation
in vec3 position;
layout(location=0) out vec4 luminance;
layout(location=1) out vec4 radianceOutput;

vec4 solarRadiance()
{
    return solarIrradianceAtTOA/(PI*sqr(sunAngularRadius));
}

void main()
{
    const vec3 viewDir=calcViewDir();

    // NOTE: we simply clamp negative altitudes to zero (otherwise the model will break down). This is not
    // quite correct physically: there are places with negative elevation above sea level. But the error of
    // this approximation has the same order of magnitude as the assumption that the Earth and its atmosphere
    // are spherical.
    float altitude = max(cameraPosition.z, 0.);
    const vec3 oldCamPos=cameraPosition;
    // Hide the uniform with this name, thus effectively modifying it for the following code
    vec3 cameraPosition=vec3(oldCamPos.xy, altitude);

    bool lookingIntoAtmosphere=true;
    if(altitude>atmosphereHeight)
    {
        const vec3 p = cameraPosition - earthCenter;
        const float p_dot_v = dot(p, viewDir);
        const float p_dot_p = dot(p, p);
        const float squaredDistBetweenViewRayAndEarthCenter = p_dot_p - sqr(p_dot_v);
        const float distanceToTOA = -p_dot_v - sqrt(sqr(earthRadius+atmosphereHeight) - squaredDistBetweenViewRayAndEarthCenter);
        if(distanceToTOA>=0)
        {
            cameraPosition += viewDir*distanceToTOA;
            altitude = atmosphereHeight;
        }
        else
        {
#if RENDERING_ANY_ZERO_SCATTERING
            lookingIntoAtmosphere=false;
#else
            luminance=vec4(0);
            radianceOutput=vec4(0);
            return;
#endif
        }
    }

    const vec3 zenith=normalize(cameraPosition-earthCenter);
    const float cosViewZenithAngle=dot(zenith,viewDir);
    const float cosSunZenithAngle =dot(zenith,sunDirection);
    const float dotViewSun=dot(viewDir,sunDirection);

    bool viewRayIntersectsGround=false;
    {
        const vec3 p = cameraPosition - earthCenter;
        const float p_dot_v = dot(p, viewDir);
        const float p_dot_p = dot(p, p);
        const float squaredDistBetweenViewRayAndEarthCenter = p_dot_p - sqr(p_dot_v);
        const float distanceToIntersection = -p_dot_v - sqrt(sqr(earthRadius) - squaredDistBetweenViewRayAndEarthCenter);
        // altitude==0 is a special case where distance to intersection calculation
        // is unreliable (has a lot of noise in its sign), so check it separately
        if(distanceToIntersection>0 || (altitude==0 && cosViewZenithAngle<0))
            viewRayIntersectsGround=true;
    }

    const vec3 sunXY=normalize(sunDirection-dot(sunDirection,zenith)*zenith);
    const vec3 viewXY=normalize(viewDir-dot(viewDir,zenith)*zenith);
    const float azimuthRelativeToSun=atan(cross(sunXY, viewXY).z, dot(sunXY, viewXY));

#if RENDERING_ZERO_SCATTERING
    vec4 radiance;
    if(viewRayIntersectsGround)
    {
        // XXX: keep in sync with the same code in computeScatteringDensity(), but don't forget about
        //      the difference in the usage of viewDir vs incDir.
        const float distToGround = distanceToGround(cosViewZenithAngle, altitude);
        const vec4 transmittanceToGround=transmittance(cosViewZenithAngle, altitude, distToGround, viewRayIntersectsGround);
        const vec3 groundNormal = normalize(zenith*(earthRadius+altitude)+viewDir*distToGround);
        const vec4 groundIrradiance = irradiance(dot(groundNormal, sunDirection), 0);
        // Radiation scattered by the ground
        const float groundBRDF = 1/PI; // Assuming Lambertian BRDF, which is constant
        radiance = transmittanceToGround*groundAlbedo*groundIrradiance*groundBRDF
                 + lightPollutionGroundLuminance*lightPollutionRelativeRadiance;
    }
    else if(dotViewSun>cos(sunAngularRadius))
    {
        if(lookingIntoAtmosphere)
            radiance=transmittanceToAtmosphereBorder(cosViewZenithAngle, altitude)*solarRadiance();
        else
            radiance=solarRadiance();
    }
    else
    {
        discard;
    }
    radiance*=solarIrradianceFixup;
    luminance=radianceToLuminance*radiance;
    radianceOutput=radiance;
#elif RENDERING_ECLIPSED_ZERO_SCATTERING
    vec4 radiance;
    const float dotViewMoon=dot(viewDir,normalize(moonPosition-cameraPosition));
    if(viewRayIntersectsGround)
    {
        // XXX: keep in sync with the similar code in non-eclipsed zero scattering rendering.
        const float distToGround = distanceToGround(cosViewZenithAngle, altitude);
        const vec4 transmittanceToGround=transmittance(cosViewZenithAngle, altitude, distToGround, viewRayIntersectsGround);
        const vec3 pointOnGround = cameraPosition+viewDir*distToGround;
        const vec4 directGroundIrradiance = calcEclipsedDirectGroundIrradiance(pointOnGround, sunDirection, moonPosition);
        // FIXME: add first-order indirect irradiance too: it's done in non-eclipsed irradiance (in the same
        // conditions: when limited to 2 orders). This should be calculated at the same time when second order
        // is: all the infrastructure is already there.
        const vec4 groundIrradiance = directGroundIrradiance;
        // Radiation scattered by the ground
        const float groundBRDF = 1/PI; // Assuming Lambertian BRDF, which is constant
        radiance = transmittanceToGround*groundAlbedo*groundIrradiance*groundBRDF
                 + lightPollutionGroundLuminance*lightPollutionRelativeRadiance;
    }
    else if(dotViewSun>cos(sunAngularRadius) && dotViewMoon<cos(moonAngularRadius(cameraPosition,moonPosition)))
    {
        if(lookingIntoAtmosphere)
            radiance=transmittanceToAtmosphereBorder(cosViewZenithAngle, altitude)*solarRadiance();
        else
            radiance=solarRadiance();
    }
    else
    {
        discard;
    }
    radiance*=solarIrradianceFixup;
    luminance=radianceToLuminance*radiance;
    radianceOutput=radiance;
#elif RENDERING_ECLIPSED_SINGLE_SCATTERING_ON_THE_FLY
    const vec4 scattering=computeSingleScatteringEclipsed(cameraPosition,viewDir,sunDirection,moonPosition,
                                                          viewRayIntersectsGround);
    vec4 radiance=scattering*currentPhaseFunction(dotViewSun);
    radiance*=solarIrradianceFixup;
    luminance=radianceToLuminance*radiance;
    radianceOutput=radiance;
#elif RENDERING_ECLIPSED_SINGLE_SCATTERING_PRECOMPUTED_RADIANCE
    const vec2 texCoords = eclipseTexVarsToTexCoords(azimuthRelativeToSun, cosViewZenithAngle, altitude,
                                                     viewRayIntersectsGround, eclipsedSingleScatteringTextureSize);
    // We don't use mip mapping here, but for some reason, on my NVidia GTX 750 Ti with Linux-x86 driver 390.116 I get
    // an artifact at the point where azimuth texture coordinate changes from 1 to 0 (at azimuthRelativeToSun crossing
    // 0). This happens when I simply call texture(eclipsedScatteringTexture, texCoords) without specifying LOD.
    // Apparently, the driver uses the derivative for some reason, even though it shouldn't.
    const vec4 scattering = textureLod(eclipsedScatteringTexture, texCoords, 0);
    vec4 radiance=scattering*currentPhaseFunction(dotViewSun);
    radiance*=solarIrradianceFixup;
    luminance=radianceToLuminance*radiance;
    radianceOutput=radiance;
#elif RENDERING_ECLIPSED_SINGLE_SCATTERING_PRECOMPUTED_LUMINANCE
    const vec2 texCoords = eclipseTexVarsToTexCoords(azimuthRelativeToSun, cosViewZenithAngle, altitude,
                                                     viewRayIntersectsGround, eclipsedSingleScatteringTextureSize);
    // We don't use mip mapping here, but for some reason, on my NVidia GTX 750 Ti with Linux-x86 driver 390.116 I get
    // an artifact at the point where azimuth texture coordinate changes from 1 to 0 (at azimuthRelativeToSun crossing
    // 0). This happens when I simply call texture(eclipsedScatteringTexture, texCoords) without specifying LOD.
    // Apparently, the driver uses the derivative for some reason, even though it shouldn't.
    const vec4 scattering = textureLod(eclipsedScatteringTexture, texCoords, 0);
    luminance=scattering*currentPhaseFunction(dotViewSun);
#elif RENDERING_ECLIPSED_DOUBLE_SCATTERING_PRECOMPUTED_RADIANCE // FIXME: we'd better do this rendering at the same time as single scattering, this could improve performance
    vec4 radiance=exp(sampleEclipseDoubleScattering4DTexture(eclipsedDoubleScatteringTextureLower,
                                                             eclipsedDoubleScatteringTextureUpper,
                                                             cosSunZenithAngle, cosViewZenithAngle, azimuthRelativeToSun,
                                                             altitude, viewRayIntersectsGround));
    radiance*=solarIrradianceFixup;
    luminance=radianceToLuminance*radiance;
    radianceOutput=radiance;
#elif RENDERING_SINGLE_SCATTERING_ON_THE_FLY
    const vec4 scattering=computeSingleScattering(cosSunZenithAngle,cosViewZenithAngle,dotViewSun,
                                                  altitude,viewRayIntersectsGround);
    vec4 radiance=scattering*currentPhaseFunction(dotViewSun);
    radiance*=solarIrradianceFixup;
    luminance=radianceToLuminance*radiance;
    radianceOutput=radiance;
#elif RENDERING_SINGLE_SCATTERING_PRECOMPUTED_RADIANCE
    const vec4 scattering = sample4DTexture(scatteringTexture, cosSunZenithAngle, cosViewZenithAngle,
                                            dotViewSun, altitude, viewRayIntersectsGround);
    vec4 radiance=scattering*currentPhaseFunction(dotViewSun);
    radiance*=solarIrradianceFixup;
    luminance=radianceToLuminance*radiance;
    radianceOutput=radiance;
#elif RENDERING_SINGLE_SCATTERING_PRECOMPUTED_LUMINANCE
    const vec4 scattering = sample4DTexture(scatteringTexture, cosSunZenithAngle, cosViewZenithAngle,
                                            dotViewSun, altitude, viewRayIntersectsGround);
    luminance=scattering*currentPhaseFunction(dotViewSun);
#elif RENDERING_MULTIPLE_SCATTERING_LUMINANCE
    luminance=sample4DTexture(scatteringTexture, cosSunZenithAngle, cosViewZenithAngle, dotViewSun, altitude, viewRayIntersectsGround);
#elif RENDERING_MULTIPLE_SCATTERING_RADIANCE
    vec4 radiance=sample4DTexture(scatteringTexture, cosSunZenithAngle, cosViewZenithAngle, dotViewSun, altitude, viewRayIntersectsGround);
    radiance*=solarIrradianceFixup;
    luminance=radianceToLuminance*radiance;
    radianceOutput=radiance;
#elif RENDERING_LIGHT_POLLUTION_RADIANCE
    vec4 radiance=lightPollutionGroundLuminance*lightPollutionScattering(altitude, cosViewZenithAngle, viewRayIntersectsGround);
    radiance*=solarIrradianceFixup;
    luminance=radianceToLuminance*radiance;
    radianceOutput=radiance;
#elif RENDERING_LIGHT_POLLUTION_LUMINANCE
    luminance=lightPollutionGroundLuminance*lightPollutionScattering(altitude, cosViewZenithAngle, viewRayIntersectsGround);
#else
#error What to render?
#endif
}
