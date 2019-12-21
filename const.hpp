#ifndef INCLUDE_ONCE_8BF515BD_53FC_406E_BD32_71E1EF0C73CC
#define INCLUDE_ONCE_8BF515BD_53FC_406E_BD32_71E1EF0C73CC

#include <limits>

constexpr auto NaN=std::numeric_limits<float>::quiet_NaN();

constexpr double astronomicalUnit=149'597'870'700.; // m
constexpr double AU=astronomicalUnit;

constexpr double sunRadius=696350e3; /* m */

constexpr char DENSITIES_SHADER_FILENAME[]="densities.frag";
constexpr char PHASE_FUNCTIONS_SHADER_FILENAME[]="phase-functions.frag";
constexpr char COMPUTE_TRANSMITTANCE_SHADER_FILENAME[]="compute-transmittance-functions.frag";
constexpr char CONSTANTS_HEADER_FILENAME[]="const.h.glsl";
constexpr char DENSITIES_HEADER_FILENAME[]="densities.h.glsl";

#endif
