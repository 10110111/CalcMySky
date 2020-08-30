#ifndef INCLUDE_ONCE_DEB2337F_9F23_4272_A108_A1F407876313
#define INCLUDE_ONCE_DEB2337F_9F23_4272_A108_A1F407876313

template<typename T>
std::string formatDeltaTime(const std::chrono::time_point<T> timeBegin, const std::chrono::time_point<T> timeEnd)
{
    const auto microsecTaken=std::chrono::duration_cast<std::chrono::microseconds>(timeEnd-timeBegin).count();
    const auto secondsTaken=1e-6*microsecTaken;
    std::ostringstream ss;
    if(secondsTaken<60)
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
