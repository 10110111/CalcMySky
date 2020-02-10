#ifndef INCLUDE_ONCE_1DB2EDC1_C687_4FFA_BFF4_D18A54BA651B
#define INCLUDE_ONCE_1DB2EDC1_C687_4FFA_BFF4_D18A54BA651B

vec4 computeSingleScattering(const float cosSunZenithAngle, const float cosViewZenithAngle,
                             const float dotViewSun, const float altitude,
                             const bool viewRayIntersectsGround);
#endif
