#version 330
#include "version.h.glsl"
#include "const.h.glsl"
#include "ground-brdf.h.glsl"
#include "common-functions.h.glsl"

float groundBRDF(const vec3 normal, const vec3 dirFromGroundToCamera, const vec3 dirFromGroundToLight)
{
    CONST vec3 V = dirFromGroundToCamera;
    CONST vec3 N = normal;
    CONST vec3 L = dirFromGroundToLight;

    const float sigma = 90;
    const float rho = 1;

    CONST float VdotN = dot(V,N);
    CONST float LdotN = dot(L,N);
    CONST float theta_r = safeAcos(VdotN);
    CONST float sigma2 = pow(sigma*PI/180,2);

    CONST vec3 Vt = V - N*VdotN;
    CONST vec3 Lt = L - N*LdotN;
    CONST float cos_phi_diff = dot(Vt==vec3(0) ? vec3(0,0,1) : normalize(Vt), // FIXME: this ?: is a hack
                                   Lt==vec3(0) ? vec3(0,0,1) : normalize(Lt));
    CONST float theta_i = safeAcos(LdotN);
    CONST float alpha = max(theta_i, theta_r);
    CONST float beta = min(theta_i, theta_r);
    if(alpha > PI/2) return 0.;

    CONST float C1 = 1 - 0.5 * sigma2 / (sigma2 + 0.33);
    float C2 = 0.45 * sigma2 / (sigma2 + 0.09);
    if(cos_phi_diff >= 0) C2 *= sin(alpha);
    else C2 *= sin(alpha) - pow(2*beta/PI,3);
    CONST float C3 = 0.125 * sigma2 / (sigma2+0.09) * pow((4*alpha*beta)/(PI*PI),2);
    CONST float L1 = rho/PI * (C1 + cos_phi_diff * C2 * tan(beta) + (1 - abs(cos_phi_diff)) * C3 * tan((alpha+beta)/2));
    CONST float L2 = 0.17 * rho*rho / PI * sigma2/(sigma2+0.13) * (1 - cos_phi_diff*(4*beta*beta)/(PI*PI));
    return L1 + L2;
}
