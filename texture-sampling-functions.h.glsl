#ifndef INCLUDE_ONCE_AF5AE9F4_8A9A_4521_838A_F8281B8FEB53
#define INCLUDE_ONCE_AF5AE9F4_8A9A_4521_838A_F8281B8FEB53
vec4 transmittanceToAtmosphereBorder(const float cosViewZenithAngle, const float altitude);
vec4 transmittanceToSun(const float cosSunZenithAngle, float altitude);
vec4 transmittance(const float cosViewZenithAngle, const float altitude, const float dist,
                   const bool viewRayIntersectsGround);
#endif
