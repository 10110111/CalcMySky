#ifndef INCLUDE_ONCE_8C4D9B35_9651_4C70_ACDF_0A37E8038295
#define INCLUDE_ONCE_8C4D9B35_9651_4C70_ACDF_0A37E8038295

vec4 computeScatteringDensity(const float cosSunZenithAngle, const float cosViewZenithAngle, const float dotViewSun,
                              const float altitude, const int scatteringOrder, const bool radiationIsFromGroundOnly);
vec4 computeMultipleScattering(const float cosSunZenithAngle, const float cosViewZenithAngle, const float dotViewSun,
                               const float altitude, const bool viewRayIntersectsGround);

#endif
