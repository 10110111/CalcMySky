#ifndef INCLUDE_ONCE_B0879E51_5608_481B_9832_C7D601BD6AB1
#define INCLUDE_ONCE_B0879E51_5608_481B_9832_C7D601BD6AB1
float distanceToAtmosphereBorder(const float cosZenithAngle, const float observerAltitude);
float distanceToNearestAtmosphereBoundary(const float cosZenithAngle, const float observerAltitude,
                                          const bool viewRayIntersectsGround);
float distanceToGround(const float cosZenithAngle, const float observerAltitude);
bool rayIntersectsGround(const float cosViewZenithAngle, const float observerAltitude);
float safeSqrt(const float x);
float clampCosine(const float x);
float clampDistance(const float x);
float clampAltitude(const float altitude);
vec4 rayleighPhaseFunction(float dotViewSun);

bool debugDataPresent();
vec3 debugData();
#endif
