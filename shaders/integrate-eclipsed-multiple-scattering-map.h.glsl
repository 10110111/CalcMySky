#ifndef INCLUDE_ONCE_5171CB97_1DAA_4108_9F5F_1C86EB6C3B34
#define INCLUDE_ONCE_5171CB97_1DAA_4108_9F5F_1C86EB6C3B34

// sunDir is not used because worldToMap transforms it to (0,0,1)
// moonPos is not used because we use uniforms describing shadow position WRT the subsolar point
vec4 integrateEclipsedMultipleScatteringMap(const vec3 camera, const vec3 viewDir, const float cosViewZenithAngle,
                                            const float cameraAltitude, const mat3 worldToMap,
                                            const bool viewRayIntersectsGround);

#endif
