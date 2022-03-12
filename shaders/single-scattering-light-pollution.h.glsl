#ifndef INCLUDE_ONCE_1544396F_0D8D_493C_8F74_321B9AECF5FD
#define INCLUDE_ONCE_1544396F_0D8D_493C_8F74_321B9AECF5FD

vec4 computeSingleScatteringForLightPollution(const vec3 cameraPos, const vec3 viewDir, const vec3 emitterPos,
                                              const float altitude, const float cosViewZenithAngle, const bool viewRayIntersectsGround);

#endif
