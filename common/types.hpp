#ifndef INCLUDE_ONCE_845F48B0_96A7_4917_936D_B8A24F58D657
#define INCLUDE_ONCE_845F48B0_96A7_4917_936D_B8A24F58D657

#include <QObject>
#include <QString>

#include "util.hpp"

enum class PhaseFunctionType
{
    General,    //!< Applied separately for each wavelength; textures are saved separately for each wavelength set
    Achromatic, //!< Can be applied to luminance instead of radiance, so textures are merged into a single XYZW texture
    Smooth,     //!< Is smooth enough to merge single scattering luminance into multiple scattering texture
};

inline QString toString(PhaseFunctionType type)
{
    switch(type)
    {
    case PhaseFunctionType::General:    return "general";
    case PhaseFunctionType::Achromatic: return "achromatic";
    case PhaseFunctionType::Smooth:     return "smooth";
    }
    return QString("bad type %1").arg(static_cast<int>(type));
}

inline PhaseFunctionType parsePhaseFunctionType(QString const& type, QString const& filename, const int lineNumber)
{
    if(type=="general")    return PhaseFunctionType::General;
    if(type=="achromatic") return PhaseFunctionType::Achromatic;
    if(type=="smooth")     return PhaseFunctionType::Smooth;
    throw ParsingError(QObject::tr("bad phase function type %1").arg(type), filename, lineNumber);
}

enum SingleScatteringRenderMode
{
    SSRM_ON_THE_FLY,
    SSRM_PRECOMPUTED,

    SSRM_COUNT
};
constexpr const char* singleScatteringRenderModeNames[SSRM_COUNT]={"on-the-fly", "precomputed"};
inline QString toString(SingleScatteringRenderMode mode) { return singleScatteringRenderModeNames[mode]; }


#endif
