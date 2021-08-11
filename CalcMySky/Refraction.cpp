#define _USE_MATH_DEFINES // for MSVC to define M_PI etc.
#include "Refraction.hpp"
#include <cmath>
#include <tuple>
#include <vector>
#include <cassert>
#include <iterator>
#include <stdexcept>
#include <boost/math/quadrature/gauss_kronrod.hpp>
#include "../common/util.hpp"

using namespace std;
using Real=Refraction::Real;

constexpr Real UNDERGROUND=-1; // m

std::tuple<Real,Real> Refraction::refractivityDerivatives(const Real altitude) const
{
    Real rate=NAN, arg=NAN;
    if(altitude < altMinInData)
    {
        // Extrapolating in the similar exp-log fashion as we interpolate
        const auto lowerAlt=altMinInData;
        const auto upperAlt=refractivityPoints[1].altitude;
        const auto lowerVal=log(refractivityPoints[0].refractivity);
        const auto upperVal=log(refractivityPoints[1].refractivity);
        rate = (upperVal-lowerVal)/(upperAlt-lowerAlt);
        arg = lowerVal+rate*(altitude-altMinInData);
    }
    else if(altitude > altMaxInData)
    {
        const auto N=refractivityPoints.size();
        const auto lowerAlt=refractivityPoints[N-2].altitude;
        const auto upperAlt=altMaxInData;
        const auto lowerVal=log(refractivityPoints[N-2].refractivity);
        const auto upperVal=log(refractivityPoints[N-1].refractivity);
        rate = (upperVal-lowerVal)/(upperAlt-lowerAlt);
        arg = upperVal+rate*(altitude-altMaxInData);
    }
    else
    {
        for(unsigned i=1; i<refractivityPoints.size(); ++i)
        {
            if(altitude<=refractivityPoints[i].altitude)
            {
                const auto lowerAlt=refractivityPoints[i-1].altitude;
                const auto upperAlt=refractivityPoints[i].altitude;
                const auto lowerVal=log(refractivityPoints[i-1].refractivity);
                const auto upperVal=log(refractivityPoints[i-0].refractivity);
                rate = (upperVal-lowerVal)/(upperAlt-lowerAlt);
                arg = lowerVal+rate*(altitude-lowerAlt);
                break;
            }
        }
    }

    const auto core = exp(arg);
    return std::make_tuple(core, rate*core);
}

Real Refraction::refractivity(const Real altitude) const
{
    return std::get<0>(refractivityDerivatives(altitude));
}

template<int orderOfDerivative>
Real Refraction::refractivityDerivative(const Real altitude) const
{
    return std::get<orderOfDerivative>(refractivityDerivatives(altitude));
}

Real Refraction::M(const Real altitude) const
{
    return (1+refractivity(altitude))*(earthRadius+altitude);
}

std::tuple<Real,Real> Refraction::mDerivatives(const Real altitude) const
{
    const auto [r0,r1]=refractivityDerivatives(altitude);
    const auto R=(earthRadius+altitude);
    return std::make_tuple((1+r0)*R, r0+R*r1);
}

Real Refraction::mPrime(const Real altitude) const
{
    return std::get<1>(mDerivatives(altitude));
}

template<typename Func>
static Real findRoot(Func f, const Real argMin, const Real argMax)
{
    auto lower=argMin, upper=argMax;

    const auto fLower=f(lower), fUpper=f(upper);
    if(fLower*fUpper>0)
        throw std::logic_error("Both argMin and argMax result in values of f having the same sign");
    if(fLower>fUpper)
        std::swap(lower,upper);

    constexpr int MAX_ITERATIONS=52;
    for(int i=0; i<MAX_ITERATIONS; ++i)
    {
        const auto mid = (lower+upper)/2;
        if(f(mid)<0)
            lower=mid;
        else
            upper=mid;
    }
    return (lower+upper)/2;
}

Real Refraction::elevationAtRayLocation(const Real cameraAltitude, const Real viewElevation, const Real rayAltitude) const
{
    const auto cosElev=M(cameraAltitude)/M(rayAltitude)*cos(viewElevation);
    assert(cosElev < 1+100*std::numeric_limits<decltype(cosElev)>::epsilon());
    return acos(min(cosElev,Real(1.)));
}

Real Refraction::geodeticAngle(const Real cameraAltitude, const Real viewElevation, const Real targetAltitude) const
{
    constexpr Real TOO_SMALL_DISTANCE = 1e-6; // = 1 mm
    if(std::abs(cameraAltitude-targetAltitude) < TOO_SMALL_DISTANCE)
    {
        // This is hopeless: our integrand will very likely divide by zero or try to sqrt a small negative number
        return 0;
    }
    const Real sign = cameraAltitude<targetAltitude ? 1. : -1.;
    const auto p0 = refractivity(cameraAltitude);
    const auto M0 = M(cameraAltitude);
    const auto cos0 = std::cos(viewElevation);
    const auto S = earthRadius+cameraAltitude;
    const auto M0sin0_2 = sqr(M0*std::sin(viewElevation));

    const auto integral = boost::math::quadrature::gauss_kronrod<Real, 15>::integrate([=](const Real t)
        {
            const auto t2 = t*t;
            const auto altitude = cameraAltitude+sign*t2;
            const auto pt = refractivity(cameraAltitude + sign*t2);
            const auto ft = t*M0*cos0 / std::sqrt(sqr(S+sign*t2)*pt*(pt+2) - S*S*p0*(p0+2) + t2*t2 + 2*sign*S*t2 + M0sin0_2);
            return ft/(earthRadius+altitude);
        },
        0, std::sqrt(sign*(targetAltitude-cameraAltitude)), 7);
    return 2*integral;
}

Real Refraction::refractionAngleSimple(const Real cameraAltitude, const Real viewElevation, const Real altMax) const
{
    return elevationAtRayLocation(cameraAltitude,viewElevation,altMax) -
           geodeticAngle(cameraAltitude,viewElevation,altMax) -
           viewElevation;
}

double Refraction::refractionAngle(const double cameraAltitude, const double viewElevation) const
{
    const Real altMaxInData=refractivityPoints.back().altitude;
    const auto altMax = altMaxInData>cameraAltitude ? altMaxInData : 1.1*cameraAltitude;

    if(viewElevation>=0)
        return refractionAngleSimple(cameraAltitude,viewElevation,altMax);

    // The altitude where the ray becomes horizontal and starts propagating upwards
    // Lower boundary is negative to allow for easy detection of ground hits.
    const auto funcToFindRootOf = [=](Real h){ return M(cameraAltitude)*cos(viewElevation)-M(h); };
    if(funcToFindRootOf(UNDERGROUND)*funcToFindRootOf(altMax) > 0)
        return NAN;

    const auto altMin=findRoot(funcToFindRootOf, UNDERGROUND, altMax);

    if(altMin<0)
        return NAN;

    // Find the geodetic angle between the camera and the point where the ray
    // is horizontal. This is the same as the case when the ray is emitted
    // horizontally and intercepted at camera altitude.
    const auto geodeticAngleAtHmin=geodeticAngle(altMin, 0, cameraAltitude);
    // This calculation is the same as in refractionAngleSimple(), but with already
    // known value of viewElevation=0 (the parameter of that function).
    const auto refractionAngleAtHmin=-geodeticAngleAtHmin-viewElevation;
    // Final refraction angle is the sum of the refraction in the downwards
    // motion of the ray, and that of the upwards motion.
    return refractionAngleAtHmin+refractionAngleSimple(altMin,0,altMax);
}

double Refraction::opticalHorizonElevation(const double altitude) const
{
    double elevMin=-M_PI/2;
    double elevMax=0;
    while(true)
    {
        const auto elevation = (elevMin+elevMax)/2;
        if(elevation==elevMin || elevation==elevMax)
            break;
        const auto refraction=refractionAngle(altitude, elevation);
        if(std::isnan(refraction))
            elevMin=elevation;
        else
            elevMax=elevation;
    }
    return elevMax;
}

Refraction::Refraction(const Real earthRadius, std::vector<RefractivityPoint> points)
    : refractivityPoints(std::move(points))
    , refractivityDataMaxAltitude(refractivityPoints.back().altitude)
    , altMinInData(refractivityPoints[0].altitude)
    , altMaxInData(refractivityPoints[refractivityPoints.size()-1].altitude)
    , earthRadius(earthRadius)
{
    // TODO: handle out-of-data extrapolation too
    for(unsigned n=0; n < refractivityPoints.size()-1; ++n)
    {
        const auto altMin=refractivityPoints[n].altitude;
        const auto altMax=refractivityPoints[n+1].altitude;
        const auto mp0=mPrime(altMin);
        const auto mp1=mPrime(altMax);

        Real rootAlt=NAN;
        if(mp0*mp1>0)
            continue; // Same sign at both ends => no root
        if(mp0==0)
            rootAlt=mp0;
        else if(mp1==0)
            rootAlt=mp1;
        else
            rootAlt=findRoot([this](Real h){return mPrime(h);}, altMin, altMax);

        assert(!std::isnan(rootAlt));

        // Interpolating the same way as in refractivityDerivative()
        const auto lowerAlt=altMin;
        const auto upperAlt=altMax;
        const auto lowerVal=log(refractivityPoints[n+0].refractivity);
        const auto upperVal=log(refractivityPoints[n+1].refractivity);
        const auto rate = (upperVal-lowerVal)/(upperAlt-lowerAlt);
        const auto arg = lowerVal+rate*(rootAlt-altMinInData);
        const auto refractivity = exp(arg);
        ++n;
        refractivityPoints.insert(refractivityPoints.begin() + n, {rootAlt, refractivity});
    }
}
