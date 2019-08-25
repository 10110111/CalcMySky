#ifndef INCLUDE_ONCE_11C0FDE7_AA5F_4B41_A8EB_4011ECDF8864
#define INCLUDE_ONCE_11C0FDE7_AA5F_4B41_A8EB_4011ECDF8864

// These are like enum entries. They are used to choose which density to calculate.
const int DENSITY_ABS_OZONE=1;
const int DENSITY_REL_RAYLEIGH=2;
const int DENSITY_REL_MIE=3;

float rayleighScattererRelativeDensity(float altitude);
float mieScattererRelativeDensity(float altitude);
float ozoneDensity(float altitude);

float density(float altitude, int whichDensity);

#endif
