#ifndef INCLUDE_ONCE_AA4CEB19_3901_49E3_A1FD_4287402FC84B
#define INCLUDE_ONCE_AA4CEB19_3901_49E3_A1FD_4287402FC84B

#include <QString>
#include <QObject>

namespace ShowMySky
{

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

}

#endif
