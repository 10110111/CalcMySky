#ifndef INCLUDE_ONCE_02040F35_0604_4759_A47D_71849AAC953D
#define INCLUDE_ONCE_02040F35_0604_4759_A47D_71849AAC953D

/** \file ShowMySky/api/ShowMySky/AtmosphereRenderer.hpp */

#include <memory>
#include <functional>

#include <QObject>
#include <QVector4D>
#include <qopengl.h>

#include "Settings.hpp"
#include "Exception.hpp"

/** \cond HIDDEN_SYMBOLS */
#ifdef SHOWMYSKY_COMPILING_SHARED_LIB
# define SHOWMYSKY_DLL_PUBLIC Q_DECL_EXPORT
#elif defined SHOWMYSKY_COMPILING_CALCMYSKY
# define SHOWMYSKY_DLL_PUBLIC
#else
# define SHOWMYSKY_DLL_PUBLIC Q_DECL_IMPORT
#endif
/** \endcond */


class QString;
class QOpenGLShaderProgram;
class QOpenGLFunctions_3_3_Core;

namespace ShowMySky
{

class AtmosphereRenderer
{
public:
    /**
     * \brief Spectral radiance of a pixel.
     */
    struct SpectralRadiance
    {
        std::vector<float> wavelengths; //!< Wavelengths in nanometers
        std::vector<float> radiances;   //!< Spectral radiance in \f$\mathrm{\frac{W}{m^2\,sr\,nm}}\f$

        float azimuth;   //!< Azimuth from which this radiance was measured, in degrees
        float elevation; //!< Elevation angle from which this radiance was measured, in degrees

        //! Number of points in the spectrum.
        unsigned size() const { return wavelengths.size(); }
        //! \c true if the spectrum has no points.
        bool empty() const { return wavelengths.empty(); }
    };

    /**
     * \brief View direction of a pixel.
     */
    struct Direction
    {
        float azimuth; //!< View azimuth, in degrees
        float elevation; //!< View elevation angle, in degrees
    };

    /**
     * \brief Status of data loading process
     */
    struct LoadingStatus
    {
        int stepsDone; //!< Number of steps done
        int stepsToDo; //!< Total number of steps to do. Negative in case of error (e.g. when a step function was called at inappropriate moment).
    };

public:
    /**
     * \brief Set the callback that will draw the screen surface.
     *
     * This method sets a callback function that will be called each time a surface is to be rendered.
     *
     * The callback has one parameter: a shader program \p shprog that includes, aside from the core atmosphere model GLSL code, the shaders \p viewDirVertShaderSrc and \p viewDirFragShaderSrc that were passed to #initDataLoading method (or to the constructor of ::AtmosphereRenderer). By the point when this callback is called, \p shprog has already been bound to current OpenGL context (via \c glUseProgram).
     *
     * The purpose of \p shprog is to enable setting of the necessary uniform values via the \c glUniform* family of functions to make the drawing work. It also includes the \c calcViewDir function implemented in \p viewDirVertShaderSrc shader, so the necessary uniform values (if any) for the operation of \c calcViewDir should also to be set in this callback before issuing any draw calls.
     *
     * \param drawSurface the callback that renders the surface.
     */
    virtual void setDrawSurfaceCallback(std::function<void(QOpenGLShaderProgram& shprog)> const& drawSurface) = 0;
    /**
     * \brief Initialize step-by-step process of loading of data.
     *
     * This method initializes the process of loading of shaders and textures for the current atmosphere model. The actual loading is done step-by-step by calling #stepDataLoading.
     *
     * The fragment shader \p viewDirFragShaderSrc passed here implements `vec3 calcViewDir(void)` function that is used as an application-agnostic way of determining view direction of the ray associated with a given fragment of the surface being rendered.
     *
     * \param viewDirVertShaderSrc a vertex shader that will be used by all the shader programs that implement the atmosphere model;
     * \param viewDirFragShaderSrc a fragment shader that implements \c calcViewDir function;
     * \param viewDirBindAttribLocations locations of vertex attributes necessary to render the screen surface. Each \c pair consists of an attribute name and its location.
     * \return Total number of loading steps to complete the loading.
     */
    virtual int initDataLoading(QByteArray viewDirVertShaderSrc, QByteArray viewDirFragShaderSrc,
                                std::vector<std::pair<std::string,GLuint>> viewDirBindAttribLocations={}) = 0;
    /**
     * \brief Perform a single step of data loading.
     *
     * This method performs a single step of data loading.
     * \return Status of data loading process: progress, error indication.
     */
    virtual LoadingStatus stepDataLoading() = 0;
    /**
     * \brief Initialize step-by-step process of preparation to render.
     *
     * The renderer loads different slices of textures depending on current scene parameters (e.g. altitude). Whenever such parameters change, it may become necessary to reload another slice.
     *
     * The application might want to show a progress indicator for this reloading process. In this case, calling this method will initialize the process if it's needed and tell whether there's anything to do (return value is positive). If the return value is positive, #stepPreparationToDraw should be called repeatedly until its return value indicates completion (or failure).
     *
     * If there's no need for progress indication, this function (and #stepPreparationToDraw) can be ignored. Otherwise, use this method to initialize the process of reloading data.
     *
     * This method can also be called during the preparation to render to find out the number of steps to do.
     *
     * \return Total number of loading steps to complete the preparation.
     */
    virtual int initPreparationToDraw() = 0;
    /**
     * \brief Perform a single step of preparation to draw.
     *
     * This method performs a single step of preparation to draw started by #initPreparationToDraw.
     * \return Status of preparation process: progress, error indication.
     */
    virtual LoadingStatus stepPreparationToDraw() = 0;
    /**
     * \brief Get a string describing current activity.
     *
     * This is a string intended for the user to see what's going on, e.g. "Loading textures and shaders...".
     */
    virtual QString currentActivity() const = 0;
    /**
     * \brief Tell whether the renderer is ready for a #draw call.
     * \returns Whether the renderer is ready to render via the #draw method.
     */
    virtual bool isReadyToRender() const = 0;
    /**
     * \brief Tell whether the renderer is in process of loading data.
     * \returns Whether the renderer is in process of loading data.
     */
    virtual bool isLoading() const = 0;
    /**
     * \brief Tell whether radiance of a pixel can be queried.
     * \returns Whether radiance of a pixel can be queried by a call to #getPixelSpectralRadiance.
     */
    virtual bool canGrabRadiance() const = 0;
    /**
     * \brief Tell whether solar spectrum can be changed.
     * \returns Whether solar spectrum can be set by a call to #setSolarSpectrum.
     */
    virtual bool canSetSolarSpectrum() const = 0;
    /**
     * \brief Tell whether precomputed eclipsed double scattering can be rendered.
     *
     * The `calcmysky` utility has an option to disable generation of the texture for second-order scattering during solar eclipse. If this option was used for generation of the current atmosphere model, this function will return \c false. In this case, it is only valid for ShowMySky::Settings::multipleScatteringEnabled to return \c true either with ShowMySky::Settings::onTheFlyPrecompDoubleScatteringEnabled returning \c true, or ShowMySky::Settings::usingEclipseShader returning \c false.
     *
     * \returns Whether precomputed eclipsed double scattering can be rendered.
     */
    virtual bool canRenderPrecomputedEclipsedDoubleScattering() const = 0;
    /**
     * \brief Get OpenGL name of the luminance render target.
     *
     * This method returns the OpenGL name suitable for use with \c glBindTexture for the render target containing photopic and scotopic luminance values packed into \c vec4 pixels (see #getPixelLuminance for details). This is the main output of #draw that can be used to finally render the data to screen in the desired color space.
     *
     * \return OpenGL name of the luminance render target.
     */
    virtual GLuint getLuminanceTexture() = 0;
    /**
     * \brief Do the drawing.
     *
     * This is the method that performs all the drawing logic.
     *
     * \param brightness relative brightness of the scene. 0 effectively means draw nothing (or NaNs in case of broken textures or shaders), 1 means normal brightness.
     * \param clear whether to clear the screen surface to zeros prior to drawing. This can be set to \c false to accumulate subsequent draws, and to \c true to draw the first render.
     */
    virtual void draw(double brightness, bool clear) = 0;
    /**
     * \brief Update render target size.
     *
     * This method should be called whenever the render target changes its size.
     *
     * \param width width of the render target;
     * \param height height of the rener target.
     */
    virtual void resizeEvent(int width, int height) = 0;
    /**
     * \brief Get luminance of a pixel.
     *
     * This method obtains luminance of the pixel specified by \p pixelPos. First three components of the 4D vector are the CIE 1931 tristimulus values in lumens, while the fourth one is the scotopic luminance in lumens (CIE 1951).
     *
     * Maximum luminous efficacy is 683.002 lm/W for photopic values and 1700.13 lm/W for scotopic ones.
     * Reference: Rapport BIPM-2019/05. Principles Governing Photometry, 2nd edition. Sections 6.2, 6.3.
     *
     * \param pixelPos pixel position in window coordinates: (0,0) corresponds to top-left point.
     * \return Luminance of the pixel specified.
     */
    virtual QVector4D getPixelLuminance(QPoint const& pixelPos) = 0;
    /**
     * \brief Get spectral radiance of a pixel.
     *
     * This method obtains spectral radiance of the pixel specified by \p pixelPos.
     *
     * \param pixelPos pixel position in window coordinates: (0,0) corresponds to top-left point.
     * \return Spectral radiance of the pixel specified.
     */
    virtual SpectralRadiance getPixelSpectralRadiance(QPoint const& pixelPos) = 0;
    /**
     * \brief Get the wavelengths used in computations.
     * \returns All the wavelengths used in computations, in nanometers.
     */
    virtual std::vector<float> getWavelengths() = 0;
    /**
     * \brief Set a custom solar spectrum.
     *
     * This method lets one change the solar spectrum that was used in computations by \c calcmysky utility, to aid easy switching between types of the illuminating star.
     *
     * This method can only be called if #canSetSolarSpectrum returns \c true.
     *
     * \param spectralIrradianceAtTOA Spectral irradiance at the top of atmosphere (i.e. at the highest altitude for which the model has data), in \f$\mathrm{\frac{W}{m^2\,sr\,nm}}\f$.
     */
    virtual void setSolarSpectrum(std::vector<float> const& spectralIrradianceAtTOA) = 0;
    /**
     * \brief Reset solar spectrum to the default.
     *
     * This method undoes what #setSolarSpectrum did, returning the current spectrum to the one used by \c calcmysky utility when generating the textures.
     */
    virtual void resetSolarSpectrum() = 0;
    /**
     * \brief Get view direction of a pixel.
     *
     * This method obtains view direction corresponding to the pixel specified by \p pixelPos.
     *
     * \param pixelPos pixel position in window coordinates: (0,0) corresponds to top-left point.
     * \return View direction of the pixel specified.
     */
    virtual Direction getViewDirection(QPoint const& pixelPos) = 0;

    virtual ~AtmosphereRenderer() = default;

    /**
     * \brief Initialize step-by-step process of reloading of shaders.
     *
     * This is a debug method. It initializes the process of reloading of shaders for the current atmosphere model. The actual reloading is done step-by-step by calling #stepShaderReloading.
     *
     * The reloading can be used to quickly update the current model when a shader has been edited or regenerated with another parameters.
     */
    virtual int initShaderReloading() = 0;
    /**
     * \brief Perform a single step of reloading of shaders.
     *
     * This is a debug method. It performs a single step of the process of reloading of shaders initialized by #initShaderReloading.
     */
    virtual LoadingStatus stepShaderReloading() = 0;
    /**
     * \brief Enable or disable a single-scattering layer.
     *
     * This is a debug option that lets one toggle a single-scattering layer associated with a particular scattering species.
     *
     * This option doesn't affect transmittance of air, it only controls whether first-order inscattered light from the species specified is included in the render.
     *
     * \param name name of the species, as given in the `Scatterer` section of the model description file;
     * \param enable whether first-order inscattered light from this species should be rendered.
     */
    virtual void setScattererEnabled(QString const& name, bool enable) = 0;
};

}

extern "C"
{
/**
 * \brief Entry point for the creator of ::AtmosphereRenderer
 *
 * This function creates an instance of ::AtmosphereRenderer in the heap and returns a pointer to it. All the parameters are forwarded to the constructor, AtmosphereRenderer::AtmosphereRenderer.
 *
 * The purpose of this function is to make it possible to link dynamically to a `dlopen`ed `ShowMySky` library instead of linking to it via the import table of the application binary.
 */
SHOWMYSKY_DLL_PUBLIC ShowMySky::AtmosphereRenderer*
    ShowMySky_AtmosphereRenderer_create(QOpenGLFunctions_3_3_Core* gl,
                                        QString const* pathToData,
                                        ShowMySky::Settings* tools,
                                        std::function<void(QOpenGLShaderProgram&)> const* drawSurface);
/**
 * \brief ABI version of the header.
 *
 * This constant denotes the ABI version of the header used at compilation time. A symbol with the same name, of type `uint32_t`, resides in the library, and is available via `dlsym`, `GetProcAddress`, `QLibrary::resolve` or a similar API provided by Qt or your OS.
 *
 * If the value of the symbol doesn't match the value of this constant, the library loaded is incompatible with the header against which the binary was compiled. Mixing incompatible header and library leads to undefined behavior.
 */
#define ShowMySky_ABI_version 14

/**
 * \brief Name of library to be dlopen()-ed
 *
 * This is the name that should be passed to dlopen-like functions. Use this macro for portability instead of hard-coding a string.
 */
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
# define SHOWMYSKY_LIB_NAME "ShowMySky-Qt5"
#else
# define SHOWMYSKY_LIB_NAME "ShowMySky-Qt6"
#endif
}

#endif
