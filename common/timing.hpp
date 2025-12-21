/*
 * CalcMySky - a simulator of light scattering in planetary atmospheres
 * Copyright © 2025 Ruslan Kabatsayev
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
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

#ifndef INCLUDE_ONCE_DEB2337F_9F23_4272_A108_A1F407876313
#define INCLUDE_ONCE_DEB2337F_9F23_4272_A108_A1F407876313

template<typename T>
std::string formatDeltaTime(const std::chrono::time_point<T> timeBegin, const std::chrono::time_point<T> timeEnd)
{
    const auto microsecTaken=std::chrono::duration_cast<std::chrono::microseconds>(timeEnd-timeBegin).count();
    const auto secondsTaken=1e-6*microsecTaken;
    std::ostringstream ss;
    if(secondsTaken < 1e-3)
    {
        ss << microsecTaken << u8" \u03bcs";
    }
    else if(secondsTaken < 1)
    {
        ss << secondsTaken*1000 << " ms";
    }
    else if(secondsTaken < 60)
    {
        ss << secondsTaken << " s";
    }
    else
    {
        auto remainder=secondsTaken;
        const auto d = int(remainder/(24*3600));
        remainder -= d*(24*3600);
        const auto h = int(remainder/3600);
        remainder -= h*3600;
        const auto m = int(remainder/60);
        remainder -= m*60;
        const auto s = std::lround(remainder);
        if(d)
            ss << d << "d";
        if(d || h)
            ss << h << "h";
        if(d || h || m)
            ss << m << "m";
        ss << s << "s";
    }
    return ss.str();
}

#endif
