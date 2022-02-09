#pragma once

namespace ShowMySky
{

class Settings
{
public:
    // Sky configuration settings
    virtual double altitude()        = 0;
    virtual double sunAzimuth()      = 0;
    virtual double sunZenithAngle()  = 0;
    // These are used for eclipses
    virtual double sunAngularRadius()  = 0;
    virtual double moonAzimuth()     = 0;
    virtual double moonZenithAngle() = 0;
    virtual double earthMoonDistance() = 0;

    // This is not needed if ShowMySky's caller code does zero order rendering itself
    virtual bool zeroOrderScatteringEnabled() = 0;
    // The dominant scattering order at daytime
    virtual bool singleScatteringEnabled() = 0;
    // This one should normally be on whenever single scattering is on, but can be disabled for debugging
    virtual bool multipleScatteringEnabled() = 0;

    // Ground luminance in cd/m^2. Should normally be zero during daytime and nonzero from evening to morning.
    virtual double lightPollutionGroundLuminance() = 0;

    // Performance-quality tradeoff settings
    virtual bool onTheFlySingleScatteringEnabled() = 0;
    virtual bool onTheFlyPrecompDoubleScatteringEnabled() = 0;

    // Debugging settings
    virtual bool textureFilteringEnabled() { return true; }

    // ShowMySky's caller code needs to decide whether it's time to use the eclipse
    // shaders—depending on sky configuration and performance requirements
    virtual bool usingEclipseShader() = 0;

    // Eclipse wants to show sky-like colors instead of the ground, this option controls whether this is enabled
    virtual bool pseudoMirrorEnabled() = 0;

    virtual ~Settings() = default;
};

}
