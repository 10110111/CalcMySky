#ifndef INCLUDE_ONCE_AA4CEB19_3901_49E3_A1FD_4287402FC84B
#define INCLUDE_ONCE_AA4CEB19_3901_49E3_A1FD_4287402FC84B

#include <QString>
#include <QObject>

namespace ShowMySky
{

class Error
{
public:
    virtual QString errorType() const = 0;
    virtual QString what() const = 0;
};

}

#endif
