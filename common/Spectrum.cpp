#include "Spectrum.hpp"
#include <cassert>

namespace
{

int posOfLargestWavelengthLessThan(const double targetWL, Spectrum const& spectrum)
{
    for(int pos=0; pos<int(spectrum.size()); ++pos)
        if(spectrum.wavelengths[pos] >= targetWL)
            return pos-1; // if first is already larger than targetWL, return -1
    return spectrum.size()-1;
}

int posOfSmallestWavelengthGreaterThan(const double targetWL, Spectrum const& spectrum)
{
    for(int pos=0; pos<int(spectrum.size()); ++pos)
        if(spectrum.wavelengths[pos] >= targetWL)
            return pos;
    return spectrum.size(); // if all are smaller than targetWL, return one past the last
}

double trapezoidArea(Spectrum const& spectrum, const int posLeft, const int posRight)
{
    const auto valueAtIntervalCenter = 0.5*(spectrum.values[posLeft]+spectrum.values[posRight]);
    const auto dlambda = spectrum.wavelengths[posRight]-spectrum.wavelengths[posLeft];
    return valueAtIntervalCenter*dlambda;
}

double integrate(Spectrum const& spectrum, const double minWL, const double maxWL)
{
    const auto leftPos=std::min(int(spectrum.size()-1), posOfSmallestWavelengthGreaterThan(minWL, spectrum));
    const auto rightPos=std::max(0, posOfLargestWavelengthLessThan(maxWL, spectrum));

    double integral=0;

    if(leftPos<=rightPos)
    {
        // Use trapezoidal rule to integrate between internal sampling points in [centerWL-wlStep/2, centerWL+wlStep/2]
        for(auto i=leftPos; i<rightPos; ++i)
            integral += trapezoidArea(spectrum, i, i+1);

        // Add the parts between the interval borders and closest internal sampling points on the left...
        if(spectrum.wavelengths[leftPos]-minWL != 0)
            integral += 0.5*(spectrum.value(minWL)+spectrum.values[leftPos])*(spectrum.wavelengths[leftPos]-minWL);
        // ... and on the right
        if(maxWL-spectrum.wavelengths[rightPos] != 0)
            integral += 0.5*(spectrum.value(maxWL)+spectrum.values[rightPos])*(maxWL-spectrum.wavelengths[rightPos]);
    }
    else // this is the case when there's no sampling point between minWL and maxWL
    {
        integral = 0.5*(spectrum.value(maxWL)+spectrum.value(minWL))*(maxWL-minWL);
    }

    return integral;
}

}

Spectrum Spectrum::resample(const double wlMin, const double wlMax, const int pointCount) const
{
    assert(!empty());
    if(minWL() > wlMin || maxWL() < wlMax)
    {
        throw ResamplingError{QString("Target wavelength range includes values outside of that of input spectrum."
                                      " Input range: [%1, %2]; output range: [%3, %4].")
                                .arg(wavelengths.front()).arg(wavelengths.back()).arg(wlMin).arg(wlMax)};
    }
    const double wlStep=(wlMax-wlMin)/(pointCount-1);
    Spectrum output;
    for(int p=0; p<pointCount; ++p)
    {
        const auto centerWL = wlMin+wlStep*p;
        const auto leftWL = std::max(minWL(), centerWL-wlStep/2);
        const auto rightWL = std::min(maxWL(), centerWL+wlStep/2);
        const auto integral = integrate(*this, leftWL, rightWL);
        output.append(centerWL, integral/(rightWL-leftWL));
    }
    return output;
}

Spectrum Spectrum::parseFromCSV(QByteArray const& data, QString const& filename, int lineNumber)
{
    std::vector<double> wavelengths, values;
    auto lines=data.split('\n');
    if(!lines.isEmpty() && lines.back()=="")
        lines.pop_back();
    for(int i=0; i<lines.size(); ++i, ++lineNumber)
    {
        const auto keyval=lines[i].split(',');
        if(keyval.size()!=2)
            throw ParsingError{filename,lineNumber,QString("bad spectrum line: expected \"key,value\" pair, got \"%1\"").arg(lines[i].constData())};
        bool okWL=false, okVal=false;
        const auto wavelength=keyval[0].trimmed().toDouble(&okWL);
        const auto value=keyval[1].trimmed().toDouble(&okVal);
        if(!okWL)
            throw ParsingError{filename,lineNumber,"failed to parse wavelength string \""+keyval[0]+"\""};
        if(!okVal)
            throw ParsingError{filename,lineNumber,"failed to parse spectrum value string \""+keyval[1]+"\""};
        if(!(wavelengths.empty() || wavelengths.back() < wavelength))
            throw ParsingError{filename,lineNumber,"wavelengths don't grow monotonically as they should"};
        wavelengths.push_back(wavelength);
        values.push_back(value);
    }
    if(values.empty())
        throw ParsingError{filename,lineNumber,"Read empty spectrum"};
    return {std::move(wavelengths), std::move(values)};
}

void Spectrum::append(const double wl, const double v)
{
    wavelengths.push_back(wl);
    values.push_back(v);
}

double Spectrum::value(const double wl) const
{
    if(size()<2 || wl<wavelengths.front() || wl>wavelengths.back())
        throw std::out_of_range("Spectrum::value");
    unsigned pos;
    for(pos=0; pos<size(); ++pos)
        if(wavelengths[pos]>=wl)
            break;
    // pos now points to the first wavelength that's greater than the desired one
    const auto smallerWL=wavelengths[pos-1];
    const auto largerWL =wavelengths[pos];
    const auto alpha=(wl-smallerWL)/(largerWL-smallerWL);
    return values[pos-1]*(1-alpha)+values[pos]*alpha;
}
