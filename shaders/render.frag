#version 330

#definitions (RENDERING_ANY_ECLIPSED_SINGLE_SCATTERING, RENDERING_ANY_LIGHT_POLLUTION, RENDERING_ANY_NORMAL_SINGLE_SCATTERING, RENDERING_ANY_SINGLE_SCATTERING, RENDERING_ANY_ZERO_SCATTERING, RENDERING_ECLIPSED_DOUBLE_SCATTERING_PRECOMPUTED_LUMINANCE, RENDERING_ECLIPSED_DOUBLE_SCATTERING_PRECOMPUTED_RADIANCE, RENDERING_ECLIPSED_SINGLE_SCATTERING_ON_THE_FLY, RENDERING_ECLIPSED_SINGLE_SCATTERING_PRECOMPUTED_LUMINANCE, RENDERING_ECLIPSED_SINGLE_SCATTERING_PRECOMPUTED_RADIANCE, RENDERING_ECLIPSED_ZERO_SCATTERING, RENDERING_LIGHT_POLLUTION_LUMINANCE, RENDERING_LIGHT_POLLUTION_RADIANCE, RENDERING_MULTIPLE_SCATTERING_LUMINANCE, RENDERING_MULTIPLE_SCATTERING_RADIANCE, RENDERING_SINGLE_SCATTERING_ON_THE_FLY, RENDERING_SINGLE_SCATTERING_PRECOMPUTED_LUMINANCE, RENDERING_SINGLE_SCATTERING_PRECOMPUTED_RADIANCE, RENDERING_ZERO_SCATTERING)

#include "version.h.glsl"
#include "const.h.glsl"
#include "calc-view-dir.h.glsl"
#include "common-functions.h.glsl"
#include_if(RENDERING_ANY_SINGLE_SCATTERING) "phase-functions.h.glsl"
#include_if(RENDERING_ANY_NORMAL_SINGLE_SCATTERING) "single-scattering.h.glsl"
#include_if(RENDERING_ANY_ECLIPSED_SINGLE_SCATTERING) "single-scattering-eclipsed.h.glsl"
#include "texture-coordinates.h.glsl"
#include "radiance-to-luminance.h.glsl"
#include_if(RENDERING_ANY_ZERO_SCATTERING) "texture-sampling-functions.h.glsl"
#include_if(RENDERING_ECLIPSED_ZERO_SCATTERING) "eclipsed-direct-irradiance.h.glsl"
#include_if(RENDERING_ANY_LIGHT_POLLUTION) "texture-sampling-functions.h.glsl"

uniform sampler3D scatteringTextureInterpolationGuides01;
uniform sampler3D scatteringTextureInterpolationGuides02;
uniform sampler3D scatteringTexture;
uniform sampler2D eclipsedScatteringTexture;
uniform sampler3D eclipsedDoubleScatteringTexture;
uniform vec3 cameraPosition;
uniform vec3 sunDirection;
uniform vec3 moonPosition;
uniform float lightPollutionGroundLuminance;
uniform vec4 solarIrradianceFixup=vec4(1); // Used when we want to alter solar irradiance post-precomputation
uniform bool pseudoMirrorSkyBelowHorizon = false;
uniform bool useInterpolationGuides=false;
in vec3 position;
layout(location=0) out vec4 luminance;
layout(location=1) out vec4 radianceOutput;

vec4 solarRadiance()
{
    return solarIrradianceAtTOA*solarIrradianceFixup/(PI*sqr(sunAngularRadius));
}

void main()
{
    vec3 viewDir=calcViewDir();
    if(length(viewDir) == 0)
        discard;

    // NOTE: we simply clamp negative altitudes to zero (otherwise the model will break down). This is not
    // quite correct physically: there are places with negative elevation above sea level. But the error of
    // this approximation has the same order of magnitude as the assumption that the Earth and its atmosphere
    // are spherical.
    float altitude = max(cameraPosition.z, 0.);
    CONST vec3 oldCamPos=cameraPosition;
    // Hide the uniform with this name, thus effectively modifying it for the following code
    vec3 cameraPosition=vec3(oldCamPos.xy, altitude);

    bool lookingIntoAtmosphere=true;
    if(altitude>atmosphereHeight)
    {
        CONST vec3 p = cameraPosition - earthCenter;
        CONST float p_dot_v = dot(p, viewDir);
        CONST float p_dot_p = dot(p, p);
        CONST float squaredDistBetweenViewRayAndEarthCenter = p_dot_p - sqr(p_dot_v);
        CONST float distanceToTOA = -p_dot_v - sqrt(sqr(earthRadius+atmosphereHeight) - squaredDistBetweenViewRayAndEarthCenter);
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

    CONST vec3 zenith=normalize(cameraPosition-earthCenter);
    float cosViewZenithAngle=dot(zenith,viewDir);

    bool viewRayIntersectsGround=false;
    {
        CONST vec3 p = cameraPosition - earthCenter;
        CONST float p_dot_v = dot(p, viewDir);
        CONST float p_dot_p = dot(p, p);
        CONST float squaredDistBetweenViewRayAndEarthCenter = p_dot_p - sqr(p_dot_v);
        CONST float distanceToIntersection = -p_dot_v - sqrt(sqr(earthRadius) - squaredDistBetweenViewRayAndEarthCenter);
        // altitude==0 is a special case where distance to intersection calculation
        // is unreliable (has a lot of noise in its sign), so check it separately
        if(distanceToIntersection>0 || (altitude==0 && cosViewZenithAngle<0))
            viewRayIntersectsGround=true;
    }

    bool viewingPseudoMirror = false;
    float pseudoMirrorDepth = 0;
    // Stellarium wants to display a sky-like view when ground is hidden.
    // Aside from aesthetics, this affects brightness input to Stellarium's tone mapper.
    if(pseudoMirrorSkyBelowHorizon && viewRayIntersectsGround)
    {
        CONST float horizonCZA = cosZenithAngleOfHorizon(altitude);
        CONST float viewElev  = asin(clampCosine(cosViewZenithAngle));
        CONST float horizElev = asin(clampCosine(horizonCZA));
        CONST float newViewElev = 2*horizElev - viewElev;
        CONST float viewDirXYnorm = length(viewDir-zenith*cosViewZenithAngle);
        if(viewDirXYnorm == 0)
        {
            viewDir = zenith;
        }
        else
        {
            // Remove original zenith-directed component
            viewDir -= zenith*cosViewZenithAngle;
            // Update cos(VZA). This change will affect all the subsequent code in this function.
            cosViewZenithAngle = sin(newViewElev);
            // Adjust the remaining (horizontal) components so that the final viewDir is normalized
            viewDir *= safeSqrt(1-sqr(cosViewZenithAngle))/viewDirXYnorm;
            // Add the new zenith-directed component
            viewDir += zenith*cosViewZenithAngle;
        }
        viewRayIntersectsGround = false;
        viewingPseudoMirror = true;

        // The first factor here, with dot(view,sun), results in removal of the
        // forward scattering peak. The second factor, with tanh, makes the
        // transition near sunset from a bit above horizon to a bit below smoother.
        // We limit the argument of tanh, because some GLSL implementations (e.g.
        // AMD Radeon RX 5700 XT) overflow when computing it and yield NaN.
        pseudoMirrorDepth = sqr(max(0, dot(viewDir, sunDirection))) *
                               tanh(min(10, 150 * (newViewElev - horizElev) / (PI/2 - horizElev)));
    }

    CONST float cosSunZenithAngle =dot(zenith,sunDirection);
    CONST float dotViewSun=dot(viewDir,sunDirection);

#if RENDERING_ANY_SINGLE_SCATTERING
    vec4 phaseFuncValue = currentPhaseFunction(dotViewSun);
    if(viewingPseudoMirror)
    {
        phaseFuncValue = mix(phaseFuncValue, 1/vec4(4*PI), pseudoMirrorDepth);
    }
#endif

    CONST vec3 sunXYunnorm=sunDirection-dot(sunDirection,zenith)*zenith;
    CONST vec3 viewXYunnorm=viewDir-dot(viewDir,zenith)*zenith;
    CONST vec3 sunXY = sunXYunnorm.x!=0 || sunXYunnorm.y!=0 || sunXYunnorm.z!=0 ? normalize(sunXYunnorm) : vec3(0);
    CONST vec3 viewXY = viewXYunnorm.x!=0 || viewXYunnorm.y!=0 || viewXYunnorm.z!=0 ? normalize(viewXYunnorm) : vec3(0);
    CONST float azimuthRelativeToSun=safeAtan(dot(cross(sunXY, viewXY), zenith), dot(sunXY, viewXY));

#if RENDERING_ZERO_SCATTERING
    vec4 radiance;
    if(viewRayIntersectsGround)
    {
        // XXX: keep in sync with the same code in computeScatteringDensity(), but don't forget about
        //      the difference in the usage of viewDir vs incDir.
        CONST float distToGround = distanceToGround(cosViewZenithAngle, altitude);
        CONST vec4 transmittanceToGround=transmittance(cosViewZenithAngle, altitude, distToGround, viewRayIntersectsGround);
        CONST vec3 groundNormal = normalize(zenith*(earthRadius+altitude)+viewDir*distToGround);
        CONST vec4 groundIrradiance = irradiance(dot(groundNormal, sunDirection), 0);
        // Radiation scattered by the ground
        CONST float groundBRDF = 1/PI; // Assuming Lambertian BRDF, which is constant
        radiance = transmittanceToGround*groundAlbedo*groundIrradiance*solarIrradianceFixup*groundBRDF
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
    luminance=radianceToLuminance*radiance;
    radianceOutput=radiance;
#elif RENDERING_ECLIPSED_ZERO_SCATTERING
    vec4 radiance;
    CONST float dotViewMoon=dot(viewDir,normalize(moonPosition-cameraPosition));
    if(viewRayIntersectsGround)
    {
        // XXX: keep in sync with the similar code in non-eclipsed zero scattering rendering.
        CONST float distToGround = distanceToGround(cosViewZenithAngle, altitude);
        CONST vec4 transmittanceToGround=transmittance(cosViewZenithAngle, altitude, distToGround, viewRayIntersectsGround);
        CONST vec3 pointOnGround = cameraPosition+viewDir*distToGround;
        CONST vec4 directGroundIrradiance = calcEclipsedDirectGroundIrradiance(pointOnGround, sunDirection, moonPosition);
        // FIXME: add first-order indirect irradiance too: it's done in non-eclipsed irradiance (in the same
        // conditions: when limited to 2 orders). This should be calculated at the same time when second order
        // is: all the infrastructure is already there.
        CONST vec4 groundIrradiance = directGroundIrradiance;
        // Radiation scattered by the ground
        CONST float groundBRDF = 1/PI; // Assuming Lambertian BRDF, which is constant
        radiance = transmittanceToGround*groundAlbedo*groundIrradiance*solarIrradianceFixup*groundBRDF
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
    luminance=radianceToLuminance*radiance;
    radianceOutput=radiance;
#elif RENDERING_ECLIPSED_SINGLE_SCATTERING_ON_THE_FLY
    CONST vec4 scattering=computeSingleScatteringEclipsed(cameraPosition,viewDir,sunDirection,moonPosition,
                                                          viewRayIntersectsGround);
    vec4 radiance=scattering*phaseFuncValue;
    radiance*=solarIrradianceFixup;
    luminance=radianceToLuminance*radiance;
    radianceOutput=radiance;
#elif RENDERING_ECLIPSED_SINGLE_SCATTERING_PRECOMPUTED_RADIANCE
    CONST vec2 texCoords = eclipseTexVarsToTexCoords(azimuthRelativeToSun, cosViewZenithAngle, altitude,
                                                     viewRayIntersectsGround, eclipsedSingleScatteringTextureSize);
    // We don't use mip mapping here, but for some reason, on my NVidia GTX 750 Ti with Linux-x86 driver 390.116 I get
    // an artifact at the point where azimuth texture coordinate changes from 1 to 0 (at azimuthRelativeToSun crossing
    // 0). This happens when I simply call texture(eclipsedScatteringTexture, texCoords) without specifying LOD.
    // Apparently, the driver uses the derivative for some reason, even though it shouldn't.
    CONST vec4 scattering = textureLod(eclipsedScatteringTexture, texCoords, 0);
    vec4 radiance=scattering*phaseFuncValue;
    radiance*=solarIrradianceFixup;
    luminance=radianceToLuminance*radiance;
    radianceOutput=radiance;
#elif RENDERING_ECLIPSED_SINGLE_SCATTERING_PRECOMPUTED_LUMINANCE
    CONST vec2 texCoords = eclipseTexVarsToTexCoords(azimuthRelativeToSun, cosViewZenithAngle, altitude,
                                                     viewRayIntersectsGround, eclipsedSingleScatteringTextureSize);
    // We don't use mip mapping here, but for some reason, on my NVidia GTX 750 Ti with Linux-x86 driver 390.116 I get
    // an artifact at the point where azimuth texture coordinate changes from 1 to 0 (at azimuthRelativeToSun crossing
    // 0). This happens when I simply call texture(eclipsedScatteringTexture, texCoords) without specifying LOD.
    // Apparently, the driver uses the derivative for some reason, even though it shouldn't.
    CONST vec4 scattering = textureLod(eclipsedScatteringTexture, texCoords, 0);
    luminance=scattering*phaseFuncValue;
#elif RENDERING_ECLIPSED_DOUBLE_SCATTERING_PRECOMPUTED_RADIANCE
    vec4 radiance=exp(sampleEclipseDoubleScattering3DTexture(eclipsedDoubleScatteringTexture,
                                                             cosSunZenithAngle, cosViewZenithAngle, azimuthRelativeToSun,
                                                             altitude, viewRayIntersectsGround));
    radiance*=solarIrradianceFixup;
    luminance=radianceToLuminance*radiance;
    radianceOutput=radiance;
#elif RENDERING_ECLIPSED_DOUBLE_SCATTERING_PRECOMPUTED_LUMINANCE
    luminance=exp(sampleEclipseDoubleScattering3DTexture(eclipsedDoubleScatteringTexture,
                                                         cosSunZenithAngle, cosViewZenithAngle, azimuthRelativeToSun,
                                                         altitude, viewRayIntersectsGround));
#elif RENDERING_SINGLE_SCATTERING_ON_THE_FLY
    CONST vec4 scattering=computeSingleScattering(cosSunZenithAngle,cosViewZenithAngle,dotViewSun,
                                                  altitude,viewRayIntersectsGround);
    vec4 radiance=scattering*phaseFuncValue;
    radiance*=solarIrradianceFixup;
    luminance=radianceToLuminance*radiance;
    radianceOutput=radiance;
#elif RENDERING_SINGLE_SCATTERING_PRECOMPUTED_RADIANCE
    vec4 scattering;
    if(useInterpolationGuides)
    {
        scattering = sample3DTextureGuided(scatteringTexture, scatteringTextureInterpolationGuides01,
                                           scatteringTextureInterpolationGuides02, cosSunZenithAngle,
                                           cosViewZenithAngle, dotViewSun, altitude, viewRayIntersectsGround);
    }
    else
    {
        scattering = sample3DTexture(scatteringTexture, cosSunZenithAngle, cosViewZenithAngle,
                                     dotViewSun, altitude, viewRayIntersectsGround);
    }
    vec4 radiance=scattering*phaseFuncValue;
    radiance*=solarIrradianceFixup;
    luminance=radianceToLuminance*radiance;
    radianceOutput=radiance;
#elif RENDERING_SINGLE_SCATTERING_PRECOMPUTED_LUMINANCE
    vec4 scattering;
    if(useInterpolationGuides)
    {
        scattering = sample3DTextureGuided(scatteringTexture, scatteringTextureInterpolationGuides01,
                                           scatteringTextureInterpolationGuides02, cosSunZenithAngle,
                                           cosViewZenithAngle, dotViewSun, altitude, viewRayIntersectsGround);
    }
    else
    {
        scattering = sample3DTexture(scatteringTexture, cosSunZenithAngle, cosViewZenithAngle,
                                     dotViewSun, altitude, viewRayIntersectsGround);
    }
    luminance=scattering * (bool(PHASE_FUNCTION_IS_EMBEDDED) ? vec4(1) : phaseFuncValue);
#elif RENDERING_MULTIPLE_SCATTERING_LUMINANCE
    luminance=sample3DTexture(scatteringTexture, cosSunZenithAngle, cosViewZenithAngle, dotViewSun, altitude, viewRayIntersectsGround);
#elif RENDERING_MULTIPLE_SCATTERING_RADIANCE
    vec4 radiance=sample3DTexture(scatteringTexture, cosSunZenithAngle, cosViewZenithAngle, dotViewSun, altitude, viewRayIntersectsGround);
    radiance*=solarIrradianceFixup;
    luminance=radianceToLuminance*radiance;
    radianceOutput=radiance;
#elif RENDERING_LIGHT_POLLUTION_RADIANCE
    vec4 radiance=lightPollutionGroundLuminance*lightPollutionScattering(altitude, cosViewZenithAngle, viewRayIntersectsGround);
    luminance=radianceToLuminance*radiance;
    radianceOutput=radiance;
#elif RENDERING_LIGHT_POLLUTION_LUMINANCE
    luminance=lightPollutionGroundLuminance*lightPollutionScattering(altitude, cosViewZenithAngle, viewRayIntersectsGround);
#else
#error What to render?
#endif
}
