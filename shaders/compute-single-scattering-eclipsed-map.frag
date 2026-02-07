#version 330
#include "version.h.glsl"
#include "const.h.glsl"
#include "phase-functions.h.glsl"
#include "common-functions.h.glsl"
#include "texture-coordinates.h.glsl"
#include "eclipsed-direct-irradiance.h.glsl"
#include "single-scattering-eclipsed.h.glsl"
#include "texture-sampling-functions.h.glsl"
#include "total-scattering-coefficient.h.glsl"

uniform int cubeSideLength; // NOTE: must be even!
uniform int eclipsedAtmoMapAltitudeLayerCount;
uniform vec3 moonPos; // should be in the XZ plane; origin is at the subsolar point on the ground
uniform float lunarShadowAngleFromSubsolarPoint;
uniform vec3 incidenceDir;
out vec4 mapOutput;

void main()
{
    vec3 zenith;
    float altitude;
    bool isGroundIrradianceLayer;
    CONST vec3 pointInMap = computeEclipsedMultipleScatteringMapPoint(cubeSideLength,
                                                                      eclipsedAtmoMapAltitudeLayerCount,
                                                                      ivec2(gl_FragCoord.xy),
                                                                      lunarShadowAngleFromSubsolarPoint,
                                                                      zenith, altitude, isGroundIrradianceLayer);
    const vec3 sunDir = vec3(0,0,1);
    if(isGroundIrradianceLayer)
    {
        // Save ground irradiance
        // XXX: Ideally we'd do this in a separate step once per eclipse phase,
        // to avoid both the branch and the division by number of integration
        // points, but since this shader is only used at the precomputation
        // state, let it be so for now.
        CONST vec4 irr = calcEclipsedDirectGroundIrradiance(pointInMap, sunDir, moonPos);
        // The 2sqrt(pi) factor compensates for the spherical harmonic that the value
        // will be multiplied by on sampling in sampleEclipseMultipleScatteringMap()
        mapOutput = irr / eclipseAngularIntegrationPoints * (2 * sqrt(PI));
    }
    else
    {
        CONST float cosIncZenithAngle = clampCosine(dot(zenith, incidenceDir));
        CONST bool incRayIntersectsGround=rayIntersectsGround(cosIncZenithAngle, altitude);
        CONST vec4 incRadianceFromAtmo = computeSingleScatteringEclipsed(pointInMap, incidenceDir, sunDir, moonPos,
                                                                         incRayIntersectsGround);

        /*
           Generally, eclipsed double scattering radiance density dse(viewDir) would be,
           in terms of eclipsed single scattering radiance sse(incDir),

           $$dse(viewDir) = \int d\Omega_{inc} sse(incDir) scatCoef(dot(viewDir, incDir)).$$

           But we are computing the coefficient of the 0th spherical harmonic of dse:

           $$dse_0 = \int d\Omega_{view} dse(viewDir) Y_0^0(viewDir).$$

           After switching the order of integration and taking into account that $Y_0^0$ doesn't depend
           on viewDir, this results in phase functions being integrated away to 1, leaving
           totalScatteringCoefficientIsotropic (scatCoefIsotr below) from the totalScatteringCoefficient
           (scatCoef above). So the final expression will be

           $$dse_0 = scatCoefIsotr \; Y_0^0 \int d\Omega_{inc} sse(incDir).$$

           For the other spherical harmonics $Y_l^m$ we can't extract them from the integral, so we get
           the more general integral after switching the order of integration:

           $$dse_l^m = \int d\Omega_{inc} sse(incDir) \int d\Omega_{view}Y_l^m(viewDir) scatCoef(dot(viewDir, incDir)).$$

           Here the inner integral is a dot product of scatCoef with the spherical harmonic $Y_l^m$, while
           scatCoef itself is a product of totalScatteringCoefficientIsotropic and the phase function. So

           $$dse_l^m = scatCoefIsotr \int d\Omega_{inc} sse(incDir) \int d\Omega_{view}Y_l^m(viewDir) phaseFunc(dot(viewDir, incDir)),$$

           or, naming the coefficient of spherical harmonic expansion of the phase function $phaseFunc_l^m$
           and noting that it depends on incDir,

           $$dse_l^m = scatCoefIsotr \int d\Omega_{inc} \, sse(incDir) \, phaseFunc_l^m(incDir).$$

           In the preparation of $phaseFunc_l^m$ as a uniform we should notice that we don't have to
           integrate the phase function for every incDir: just expand it in spherical harmonics once
           and then use the corresponding Wigner matrix to rotate it according to incDir.
         */
        // TODO: implement higher-order spherical harmonics for better results
        CONST float sphericalHarmonicY0 = 1 / (2 * sqrt(PI));
        CONST vec4 scatCoefSpherHarOrder0 = sphericalHarmonicY0 * totalScatteringCoefficientIsotropic(altitude);

        CONST float dSolidAngleOfIncDirs = sphereIntegrationSolidAngleDifferential(eclipseAngularIntegrationPoints);
        mapOutput = incRadianceFromAtmo * scatCoefSpherHarOrder0 * dSolidAngleOfIncDirs;
    }
}
