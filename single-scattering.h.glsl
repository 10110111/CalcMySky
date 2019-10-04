#ifndef INCLUDE_ONCE_1DB2EDC1_C687_4FFA_BFF4_D18A54BA651B
#define INCLUDE_ONCE_1DB2EDC1_C687_4FFA_BFF4_D18A54BA651B

struct ScatteringSpectra
{
    vec4 rayleigh;
    vec4 mie;
};

ScatteringSpectra computeSingleScattering(const float cosSunZenithAngle, const float altitude, const float dotViewSun,
                                          const float cosViewZenithAngle, const bool viewRayIntersectsGround);
#endif
