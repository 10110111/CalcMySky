#ifndef INCLUDE_ONCE_AA4CEB19_3901_49E3_A1FD_4287402FC84B
#define INCLUDE_ONCE_AA4CEB19_3901_49E3_A1FD_4287402FC84B

#include <QString>
#include <QObject>

namespace ShowMySky
{

/* We use GCC-(and clang-)specific pragma instead of SHOWMYSKY_DLL_PUBLIC, because:
 * 1. This declaration as public is not needed on MSVC;
 * 2. If SHOWMYSKY_DLL_PUBLIC is used, it results in __declspec(dllimport)
 * conflicting with declaration of the derivative classes, which leads to
 * LNK4217 and LNK4049 warnings, and finally LNK2001 error.
 * We still do set default visibility, because on macOS this symbol being
 * hidden prevents the exceptions from being caught when emitted from the
 * library and expected in an application.
 */
#ifdef __GNUC__
# pragma GCC visibility push(default)
#endif
/**
 * \brief An error that ShowMySky classes may throw.
 */
class Error
{
public:
    //! \brief A string suitable for use as a title of a message box.
    virtual QString errorType() const = 0;
    //! \brief A description of the error.
    virtual QString what() const = 0;
};
#ifdef __GNUC__
# pragma GCC visibility pop
#endif

}

#endif
