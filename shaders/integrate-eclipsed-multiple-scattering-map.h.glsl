#ifndef INCLUDE_ONCE_5171CB97_1DAA_4108_9F5F_1C86EB6C3B34
#define INCLUDE_ONCE_5171CB97_1DAA_4108_9F5F_1C86EB6C3B34

vec4 integrateEclipsedMultipleScatteringMap(const vec3 camera, const vec3 sunDir, const vec3 viewDir,
                                            const vec3 moonPos, const float cosViewZenithAngle,
                                            const float cameraAltitude, const bool viewRayIntersectsGround);

#endif
