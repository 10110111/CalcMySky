#version 330
#include "version.h.glsl"
#include "const.h.glsl"
#include "phase-functions.h.glsl"
#include "common-functions.h.glsl"
#include "single-scattering-eclipsed.h.glsl"

uniform int cubeSideLength; // NOTE: must be even!
uniform int eclipsedAtmoMapAltitudeLayerCount;
uniform vec3 moonPos; // should be in the XZ plane; origin is at the subsolar point on the ground
uniform vec3 incidenceDir;
out vec4 mapOutput;

vec3 computeMapPoint(out vec3 zenith, out float altitude)
{
    /* Each altitude layer of the map is organized as follows (x marks theoretically present values
       that aren't stored due to the symmetry; number at the beginning indicates offset of the
       subtexture in the horizontal line of textures):

      azimuth=0° (with respect to the Moon)
           |
           v_____
      xxxxx|     |
      xxxxx|  0  |            azimuth=180°
      xxxxx| Zen-|azimuth=90°     |
      xxxxx| ith |    |           |
      xxxxx|_____|____v_____ _____v
      xxxxx|     |          |     |xxxxx
      xxxxx|  1  |    3     |  4  |xxxxx
      xxxxx|     |          |     |xxxxx
      xxxxx|     |          |     |xxxxx
      xxxxx|_____|__________|_____|xxxxx
      xxxxx|     |
      xxxxx|  2  |
      xxxxx| Nad-|
      xxxxx| ir  |
      xxxxx|_____|

      I.e. we get the following layout:
       __________________________________
      |     |     |     |          |     |
      |  0  |  1  |  2  |    3     |  4  |
      | Zen-|     | Nad-|          |     |
      | ith |     | ir  |          |     |
      |_____|_____|_____|__________|_____|

       So for a cubemap with cube side of W×H = 50px × 50px we'll have a texture layer 150px × 50px.
       These layers are then stacked vertically in N altitude rows to yield a texture of size W × (H*N).
       So for e.g. 50 altitude layers N = 50px, and we'll get a texture 150px × 2500px.

     */
    CONST int x = int(gl_FragCoord.x);
    CONST int y = int(gl_FragCoord.y);
    CONST int halfCubeSide = cubeSideLength / 2;
    CONST int altitudeLayerIndex = y / cubeSideLength;
    CONST int yInAltLayer = y % cubeSideLength - halfCubeSide;
    int cubeSector = x / halfCubeSide;
    int xInCubeSide;
    switch(cubeSector)
    {
    case 3:
    case 4:
        cubeSector = 3;
        xInCubeSide = x - 4 * halfCubeSide;
        break;
    case 5:
        cubeSector = 4;
        xInCubeSide = x - 6 * halfCubeSide;
        break;
    default:
        xInCubeSide = x - cubeSector * halfCubeSide;
        break;
    }

    zenith = vec3(0);
    switch(cubeSector)
    {
    case 0:
        zenith = normalize(vec3(-yInAltLayer,
                                xInCubeSide,
                                halfCubeSide));
        break;
    case 1:
        zenith = normalize(vec3(halfCubeSide,
                                xInCubeSide,
                                yInAltLayer));
        break;
    case 2:
        zenith = normalize(vec3(yInAltLayer,
                                xInCubeSide,
                                -halfCubeSide));
        break;
    case 3:
        zenith = normalize(vec3(-xInCubeSide,
                                halfCubeSide,
                                yInAltLayer));
        break;
    case 4:
        zenith = normalize(vec3(-halfCubeSide,
                                -xInCubeSide,
                                yInAltLayer));
        break;
    }

    altitude = sqr(float(altitudeLayerIndex) / eclipsedAtmoMapAltitudeLayerCount) * atmosphereHeight;
    // All points are now computed relative to the subsolar point
    CONST vec3 mapPoint = zenith * (earthRadius + altitude) + earthCenter;
    return mapPoint;
}

void main()
{
    vec3 zenith;
    float altitude;
    CONST vec3 pointInMap = computeMapPoint(zenith, altitude);
    const vec3 sunDir = vec3(0,0,1);
    CONST float cosIncZenithAngle = clampCosine(dot(zenith, incidenceDir));
    CONST bool incRayIntersectsGround=rayIntersectsGround(cosIncZenithAngle, altitude);
    CONST vec4 sample = computeSingleScatteringEclipsed(pointInMap, incidenceDir, sunDir, moonPos,
                                                        incRayIntersectsGround);

    // TODO: implement higher-order spherical harmonics for better results
    CONST float sphericalHarmonicY = 1 / (2 * sqrt(PI));

    CONST float dSolidAngle = sphereIntegrationSolidAngleDifferential(eclipseAngularIntegrationPoints);
    mapOutput = sphericalHarmonicY * sample * dSolidAngle;
}
