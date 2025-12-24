/*
 * CalcMySky - a simulator of light scattering in planetary atmospheres
 * Copyright © 2025 Ruslan Kabatsayev
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335, USA.
 */

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
