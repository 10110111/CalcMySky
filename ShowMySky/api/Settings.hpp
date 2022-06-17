#pragma once

namespace ShowMySky
{

/**
 * \brief Scene settings for AtmosphereRenderer
 */
class Settings
{
public:
    /**
     * \brief Camera altitude.
     *
     * \returns Camera altitude in meters.
     */
    virtual double altitude()        = 0;
    /**
     * \brief Azimuth of the Sun.
     *
     * \returns Azimuth of the Sun at the location of the camera in radians.
     */
    virtual double sunAzimuth()      = 0;
    /**
     * \brief Zenith angle of the Sun.
     *
     * \returns Zenith angle of the Sun at the location of the camera in radians.
     */
    virtual double sunZenithAngle()  = 0;

    /**
     * \brief Angular radius of the Sun.
     *
     * This option is used when #usingEclipseShader returns \c true.
     *
     * \returns Angular radius of the Sun when looked from the location of the camera, in radians.
     */
    virtual double sunAngularRadius()  = 0;
    /**
     * \brief Azimuth of the Moon.
     *
     * This option is used when #usingEclipseShader returns \c true.
     *
     * \returns Azimuth of the Moon at the location of the camera in radians.
     */
    virtual double moonAzimuth()     = 0;
    /**
     * \brief Zenith angle of the Moon.
     *
     * This option is used when #usingEclipseShader returns \c true.
     *
     * \returns Zenith angle of the Moon at the location of the camera in radians.
     */
    virtual double moonZenithAngle() = 0;
    /**
     * \brief Distance between the Earth and the Moon.
     *
     * This option is used when #usingEclipseShader returns \c true.
     *
     * \returns Distance between the centers of the Earth and the Moon in meters.
     */
    virtual double earthMoonDistance() = 0;

    /**
     * \brief Whether to render zero-order scattering.
     *
     * If this method returns \c true, zero-order scattering is rendered by AtmosphereRenderer::draw, otherwise it's skipped.
     *
     * This should return \c false if the application does zero-order rendering by its own means.
     */
    virtual bool zeroOrderScatteringEnabled() = 0;
    /**
     * \brief Whether to render single scattering.
     *
     * If this method returns \c true, single scattering layers are rendered by AtmosphereRenderer::draw, otherwise they are skipped.
     */
    virtual bool singleScatteringEnabled() = 0;
    /**
     * \brief Whether to render multiple scattering.
     *
     * If this method returns \c true, multiple scattering layer is rendered by AtmosphereRenderer::draw, otherwise it's skipped.
     */
    virtual bool multipleScatteringEnabled() = 0;

    /**
     * \brief Ground luminance for light pollution.
     *
     * \return Ground luminance in \f$\mathrm{cd/m^2}\f$. Should normally be zero during daytime and nonzero from evening to morning.
     */
    virtual double lightPollutionGroundLuminance() = 0;

    /**
     * \brief Whether single scattering should be computed on the fly.
     *
     * This is a performance-quality tradeoff setting.
     *
     * If this method returns \c true, single scattering is computed during rendering, on the fly. Otherwise the single scattering textures are used.
     */
    virtual bool onTheFlySingleScatteringEnabled() = 0;
    /**
     * \brief Whether double scattering should be precomputed on the fly when using eclipse shader.
     *
     * This is a performance-quality tradeoff setting.
     *
     * If this method returns \c true, and #usingEclipseShader returns \c true, then double scattering is precomputed on the fly. Otherwise it is rendered from double scattering texture (see caveats described in AtmosphereRenderer::canRenderPrecomputedEclipsedDoubleScattering).
     */
    virtual bool onTheFlyPrecompDoubleScatteringEnabled() = 0;

    /**
     * \brief Whether to enable texture filtering.
     *
     * This is a debugging option that lets one toggle linear interpolation between texels in the model textures. If \c true, filtering is enabled, otherwise disabled. It should normally be enabled.
     *
     * \returns Whether to enable texture filtering.
     */
    virtual bool textureFilteringEnabled() { return true; }

    /**
     * \brief Whether to use shader designed to render eclipse atmosphere.
     *
     * Eclipsed atmosphere takes more resources to render, so if there's no eclipse, this method should return \c false. But when the solar shadow touches the Earth, this method should return \c true (see caveats described in AtmosphereRenderer::canRenderPrecomputedEclipsedDoubleScattering).
     */
    virtual bool usingEclipseShader() = 0;

    /**
     * \brief Whether to mirror the sky instead of rendering the ground.
     *
     * Stellarium wants to show sky-like colors instead of the ground, this option controls whether this is enabled.
     *
     * \returns Whether to mirror the sky instead of rendering the ground.
     */
    virtual bool pseudoMirrorEnabled() = 0;

    virtual ~Settings() = default;
};

}
