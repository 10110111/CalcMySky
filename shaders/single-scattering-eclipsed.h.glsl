#ifndef INCLUDE_ONCE_050024D2_BD56_434E_8D40_2055DA0B78EC
#define INCLUDE_ONCE_050024D2_BD56_434E_8D40_2055DA0B78EC

vec4 computeSingleScatteringEclipsed(const vec3 camera, const vec3 viewDir, const vec3 sunDir, const vec3 moonDir,
                                     const bool viewRayIntersectsGround);
vec4 computeSingleScatteringEclipsedSample(const int depthIndex, const vec3 camera, const vec3 viewDir,
                                           const vec3 sunDir, const vec3 moonPos, const bool viewRayIntersectsGround);

#endif
