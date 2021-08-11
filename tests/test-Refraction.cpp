#define _USE_MATH_DEFINES // for MSVC to define M_PI etc.
#include "../CalcMySky/Refraction.hpp"
#include <cmath>
#include <string>
#include <iostream>
#include <stdexcept>

using Real=double;

Real getDouble(std::string const& str)
{
    std::size_t pos=0;
    const auto value=std::stod(str, &pos);
    if(pos!=str.size())
        throw std::invalid_argument("Trailing characters after floating-point number \""+str+"\"");
    return value;
}

bool checkForMatch(const Real value, const Real reference, const Real tolerance, const char*const description)
{
    const auto diff = std::abs(value-reference);
    if(diff > tolerance)
    {
        std::cerr << description << " doesn't match within tolerance of " << tolerance
                  << ": expected " << reference << ", got " << value << ", diff: " << diff << "\n";
        return false;
    }
    return true;
}

int main(int argc, char** argv) try
{
    if(argc!=4)
    {
        std::cerr << "Usage: " << argv[0]
                  << " cameraAltitudeInKM viewElevationInDeg expectedRefractionAngleInDeg\n";
        return 1;
    }

    constexpr double km=1000;
    constexpr Real degree = M_PI/180;
    constexpr Real REFRACTION_ANGLE_TOLERANCE = 1e-12;
    constexpr Real earthRadius=6371*km;

    std::cout.precision(std::numeric_limits<Real>::digits10);
    std::cerr.precision(std::numeric_limits<Real>::digits10);
    const auto cameraAltitude          = getDouble(argv[1])*km;
    const auto viewElevation           = getDouble(argv[2])*degree;
    const auto expectedRefractionAngle = getDouble(argv[3])*degree;

    Refraction refraction(earthRadius, {{0, 27717}, {120, 0.000556377166560651}});
    const auto angle=refraction.refractionAngle(cameraAltitude, viewElevation);
    const bool ok = checkForMatch(angle,expectedRefractionAngle,REFRACTION_ANGLE_TOLERANCE,"Refraction angle");
    if(!ok) return 1;
}
catch(std::exception const& ex)
{
    std::cerr << ex.what() << "\n";
    return 1;
}

