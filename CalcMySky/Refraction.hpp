#ifndef INCLUDE_ONCE_62D81AF5_D3F1_44F7_9C91_AA201C6F382A
#define INCLUDE_ONCE_62D81AF5_D3F1_44F7_9C91_AA201C6F382A

#include <vector>

class Refraction
{
public:
    using Real=double;

    struct RefractivityPoint
    {
        Real altitude;
        Real refractivity;

        RefractivityPoint(const Real altitudeInKM, const Real refractivityTimes1e8)
            : altitude(altitudeInKM * 1000)
            , refractivity(refractivityTimes1e8 * 1e-8)
        {}
    };

    Refraction(Real earthRadius, std::vector<RefractivityPoint> points);
    double refractionAngle(double cameraAltitude, double viewElevation) const;
    double opticalHorizonElevation(double altitude) const;

private:
    std::tuple<Real,Real> refractivityDerivatives(Real altitude) const;
    Real refractivity(Real altitude) const;
    template<int orderOfDerivative> Real refractivityDerivative(Real altitude) const;
    Real M(Real altitude) const;
    std::tuple<Real,Real> mDerivatives(Real altitude) const;
    Real mPrime(Real altitude) const;
    Real elevationAtRayLocation(Real cameraAltitude, Real viewElevation, Real rayAltitude) const;
    Real geodeticAngle(Real cameraAltitude, Real viewElevation, Real targetAltitude) const;
    Real refractionAngleSimple(Real cameraAltitude, Real viewElevation, Real altMax) const;

private:
    std::vector<RefractivityPoint> refractivityPoints;
    Real refractivityDataMaxAltitude;
    Real altMinInData;
    Real altMaxInData;
    Real earthRadius;
};

#endif
