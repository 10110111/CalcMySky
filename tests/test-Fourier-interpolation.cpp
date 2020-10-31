#include <limits>
#include <iostream>
#include "../common/fourier-interpolation.hpp"

constexpr float interpolationAbsoluteTolerance=1e-5;
#define FAIL(details) { std::cerr << __FILE__ << ":" << __LINE__  << ": test failed: " << details << "\n"; return 1; }

// Uniformly distributed random reals in [-5,5]
std::vector<float> input{-1.04764028364545, -2.27389807797124, -4.53480340837506, -2.31683853947485, 3.05359561429397,
    4.62757699122398, -1.84843641685465, -1.65566214095604, 3.60323249339727, 4.22514109151549, -4.01502060318266,
    3.07884923126757, -0.636691933657245, 2.41514550287572, 1.66435831435741, 0.17544277911703, 3.01608466397048,
    3.10963742405011, -0.666627953111005, -3.84316099654238, 1.28604688036524, -4.4880625915694, 4.83129025445803,
    1.03170423646597, 2.40894675419466, 2.32090065393179, 2.18062628833632, -3.71920499489401, -0.684323485541121,
    -0.0138935656158634, 0.251261651471781, -1.81024436472041, -3.03373311447034, -1.13931366363091, -0.845905730877303,
    4.09712229697328, 4.01039365591914, 2.07538130092303, 0.198754498255047, -4.75718353351595, -2.02618719402816,
    3.71415959798093, 4.00366629791064, 4.66020629456629, -4.0187018939149, -0.458445682623747, 2.25306664357397,
    -3.17923232238781, 3.0852725889165, -1.81054706031627, 2.8801943396812, 1.91967438606113, 2.80531352621946,
    -0.595715454763, -2.35613009855963, -4.80897734622888, -4.18867863081531, 2.74461364636766, 2.50237205489555,
    -2.27253484655199, 4.95036154972611, -1.06383740162795, -4.48796656889142, 0.801028670669757, 4.96625515601086,
    0.737687161576646, 1.58148231007911, 1.30854918485291, 4.91063139338994, 3.8327014786554, 2.32078198820823,
    -4.229552934696, 0.935970335980096, -0.0751791405632774, 4.80194215325841, -4.91851543227509, 4.75803701305516,
    -1.30980776831236, 3.73801271189482, -1.47795917109291, -4.92744682317781, 4.79487486622777, -2.80046117461373,
    -2.85946194392174, 2.19220530689472, 4.8110192017856, -0.970425193861961, -1.4657641787978, 1.44145015438117,
    0.0193775486318826, 0.138038937641429, -1.8003948325443, -1.05716557055535, 4.86245134947316, -1.59451122974651,
    -4.43102094968848, 4.41816995942295, 3.0736750689687, 2.81506995475357, 3.45105535784897, 3.68629445339814,
    -3.76027668801237, -2.4138527734768, -2.76968115245899, 0.939926232352064, -2.12548724464433, 3.06509401081305,
    -1.93430058512952, 0.332168047423664, -0.721355582841656, -4.88477841903451, 1.2191674956769, 4.22310665713527,
    2.44613620938995, -2.91405322733154, -4.49480648440628, -2.20793695371158, 2.38793661943974, -2.65547845176471,
    -1.91171828068158, 2.46561385573153, -4.97688846768933, 4.66782635255643, 2.24691463302909, 2.44806301807539,
    1.29509430810062, 3.30156084995913, -1.6021688171486, -3.80148639735339, 3.82858078572897, -3.67380159366597,
    0.150407918831741, 0.612434537653077, 4.14127425212816, -2.72168223704007, 0.0662110167514096, 0.710606095129711,
    -2.32084749318826, 4.88644732733789, -0.987957409460425, 1.05323493074359, 3.8693180805419, -1.75739632377227,
    -3.44614770497044, -1.86439610059509, -2.7577676213795, 0.632603277725052, 4.03471948589203, 0.219753050697062,
    -4.59457221615793, -0.793632258250203, 4.77146517359489, -4.19564494174838, -2.80267317321552, 4.19788879349541,
    0.0355570716886344, 4.8930940883707, 4.83304614159602, 4.33553307402602, 3.51221688460522, 3.33476949654069,
    4.85085762105681, -0.437957655845054, -1.36648665453177, 2.62906393142441, 4.34550094034477, -0.688950097866568,
    0.429433273788056, 3.59541117753626, -3.81304309173514, 3.59723050872023, 1.41916816470139, -3.14306156264406,
    4.62092538624516, -1.12663470499742, 3.61233244908165, -0.41082111355023, -3.05480175863048, -2.06475820759664,
    0.396901610846403, 0.00258004814206458, 0.00760735357037357, -1.40824320497397, -0.871342644550135,
    4.59214151423347, -2.20505927814476, 4.33366854204197, 0.752453914440836, 2.83766605243883, 3.31928602443609,
    -2.4405820132074, 3.84661933915121, -2.56023810266499, 2.60235817686796, 2.76677319223049, -1.50440227378409,
    0.351025078032681, 2.90939268298726, -3.92352713688308, -3.27135394474332, 3.26555163534195, 2.48824241724644,
    -1.69013290722925, 3.41721604947382, -4.06718250616174, -4.43352066833075, -1.07951525191261, -4.22191291882816,
    -0.903216426439062, 3.53302427169594};
int testIdentityTransformation(const bool oddInputSize)
{
    if(int(oddInputSize) != input.size()%2)
        input.pop_back();

    std::vector<float> interpolated(input.size());
    std::vector<std::complex<float>> intermediate(interpolated.size());
    fourierInterpolate(input.data(), input.size(), intermediate.data(), interpolated.data(), interpolated.size());

    for(unsigned k=0; k<input.size(); ++k)
    {
        const auto diff = input[k]-interpolated[k];
        if(std::abs(diff) > interpolationAbsoluteTolerance)
            FAIL("output value at index " << k << " differs from input by " << diff << ", which is more than "
                 << interpolationAbsoluteTolerance << "\n");
    }

    return 0;
}

int testIntegralUpsampling(const bool oddInputSize)
{
    if(int(oddInputSize) != input.size()%2)
        input.pop_back();

    const auto scale=3;
    std::vector<float> interpolated(scale*input.size());
    std::vector<std::complex<float>> intermediate(interpolated.size());
    fourierInterpolate(input.data(), input.size(), intermediate.data(), interpolated.data(), interpolated.size());
    for(unsigned inputIndex=0; inputIndex<input.size(); ++inputIndex)
    {
        const auto outputIndex = scale*inputIndex;
        const auto diff = input[inputIndex]-interpolated[outputIndex];
        if(std::abs(diff) > interpolationAbsoluteTolerance)
            FAIL("output value at index " << outputIndex << " differs from input at index " << inputIndex << " by "
                 << diff << ", which is more than " << interpolationAbsoluteTolerance << "\n");
    }

    return 0;
}

int testFractionalUpsampling(const bool oddInputSize)
{
    if(int(oddInputSize) != input.size()%2)
        input.pop_back();

    const unsigned scaleInputToLarge = oddInputSize ? 19 : 7;
    const unsigned scaleSmallToLarge = oddInputSize ? 11 : 3;
    const unsigned largeSize = input.size()*scaleInputToLarge;
    const unsigned smallSize = largeSize/scaleSmallToLarge;
    if(largeSize%smallSize!=0)
        FAIL("output sizes chosen are not one multiple of another: small " << smallSize << ", large " << largeSize);

    std::vector<float> interpolatedSmall(smallSize);
    std::vector<std::complex<float>> intermediate(interpolatedSmall.size());
    fourierInterpolate(input.data(), input.size(), intermediate.data(), interpolatedSmall.data(), interpolatedSmall.size());

    std::vector<float> interpolatedLarge(largeSize);
    intermediate.resize(interpolatedLarge.size());
    fourierInterpolate(input.data(), input.size(), intermediate.data(), interpolatedLarge.data(), interpolatedLarge.size());

    // 1. interpolatedLarge must repeat each scaleInputToLarge element of input
    for(unsigned inputIndex=0; inputIndex<input.size(); ++inputIndex)
    {
        const auto outputIndex = scaleInputToLarge*inputIndex;
        const auto diff = input[inputIndex]-interpolatedLarge[outputIndex];
        if(std::abs(diff) > interpolationAbsoluteTolerance)
            FAIL("value of large-length output at index " << outputIndex << " differs from input at index " << inputIndex
                 << " by " << diff << ", which is more than " << interpolationAbsoluteTolerance << "\n");
    }

    // 2. interpolateLarge must also repeat each scaleSmallToLarge element of interpolatedSmall
    for(unsigned smallIndex=0; smallIndex<interpolatedSmall.size(); ++smallIndex)
    {
        const auto largeIndex = scaleSmallToLarge*smallIndex;
        const auto diff = interpolatedSmall[smallIndex]-interpolatedLarge[largeIndex];
        if(std::abs(diff) > interpolationAbsoluteTolerance)
            FAIL("value of large-length output at index " << largeIndex << " differs from value of small-length output at index "
                 << smallIndex << " by " << diff << ", which is more than " << interpolationAbsoluteTolerance << "\n");
    }

    return 0;
}

int main(int argc, char** argv)
{
    std::cerr.precision(std::numeric_limits<float>::max_digits10);

    if(argc!=3)
    {
        std::cerr << "Which test to run, with what parity of input size?\n";
        return 1;
    }

    const std::string arg=argv[1];
    const std::string parity=argv[2];
    const bool odd = parity=="odd";
    if(parity!="odd" && parity!="even")
    {
        std::cerr << "Parity must be even or odd, but command line says \"" << parity << "\"\n";
        return 1;
    }
    if(arg=="identity transformation")
        return testIdentityTransformation(odd);
    if(arg=="integral upsampling")
        return testIntegralUpsampling(odd);
    if(arg=="fractional upsampling")
        return testFractionalUpsampling(odd);

    std::cerr << "Unknown test " << arg << "\n";
    return 1;
}
