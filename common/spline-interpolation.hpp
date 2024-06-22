#ifndef INCLUDE_ONCE_F820C110_1DC9_40B4_8442_EDD0227CB7E8
#define INCLUDE_ONCE_F820C110_1DC9_40B4_8442_EDD0227CB7E8

#include <Eigen/Dense>
#include <cassert>
#include <vector>

template<typename Number, typename Vec2>
class SplineOrder2InterpolationFunction
{
public:
    struct Chunk
    {
        Number xMax; // right border of the chunk's domain of definition
        Number a, b, c; // a x^2 + b x + c
        Chunk(Number xMax, Number a, Number b, Number c)
            : xMax(xMax)
            , a(a), b(b), c(c)
        {}
    };
    SplineOrder2InterpolationFunction()=default;
    SplineOrder2InterpolationFunction(std::vector<Chunk>&& chunks) : chunks(chunks) {}
    Number sample(Number const x) const
    {
        assert(!chunks.empty());

        std::size_t chunkIndex;
        for(chunkIndex=0; chunkIndex<chunks.size() && x > chunks[chunkIndex].xMax; ++chunkIndex);

        if(chunkIndex==chunks.size())
            throw std::out_of_range("SplineOrder2InterpolationFunction::sample: too large x");

        const auto a = chunks[chunkIndex].a;
        const auto b = chunks[chunkIndex].b;
        const auto c = chunks[chunkIndex].c;
        return a*x*x + b*x + c;
    }
private:
    std::vector<Chunk> chunks;
};

template<typename Vec2, typename Number=typename std::remove_cv<typename std::remove_reference<decltype(Vec2().x)>::type>::type>
SplineOrder2InterpolationFunction<Number,Vec2> splineInterpolationOrder2(Vec2 const*const points, const std::size_t pointCount)
{
    assert(pointCount>=3);
    assert(std::is_sorted(points,points+pointCount,[](Vec2 const& a, Vec2 const& b){return a.x<b.x;}));

    const auto sqr=[](Number x){ return x*x; };
    enum { A=0, B=1, C=2 };

    const int n=pointCount;
    const int N=3*(n-2);

    using namespace Eigen;
    using Matrix=Eigen::Matrix<Number, Dynamic, Dynamic>;
    using Vector=Eigen::Matrix<Number, Dynamic, 1>;
    Matrix M=Matrix::Zero(N, N);
    Vector R=Vector::Zero(N);

    // All indices in the comments are 1-based, the equations are written in Wolfram Language

    // Values of first and last functions at endpoints must equal ordinates of corresponding endpoint.
    // This gives two equations. First:
    //  a[1] points[[1, 1]]^2 + b[1] points[[1, 1]] + c[1] == points[[1, 2]]
    /*a[1]*/M(0, 3*0+A)=sqr(points[0].x);
    /*b[1]*/M(0, 3*0+B)=    points[0].x ;
    /*c[1]*/M(0, 3*0+C)=1;
    /*RHS*/ R(0)=points[0].y;
    // And second:
    //  a[n - 2] points[[n, 1]]^2 + b[n - 2] points[[n, 1]] + c[n - 2] == points[[n, 2]]
    /*a[n-2]*/M(1, 3*(n-2-1)+A)=sqr(points[n-1].x);
    /*b[n-2]*/M(1, 3*(n-2-1)+B)=    points[n-1].x ;
    /*c[n-2]*/M(1, 3*(n-2-1)+C)=1;
    /* RHS */ R(1)=points[n-1].y;

    // Value of ith function at (i + 1)th point must be equal to the point ordinate.
    // This gives (n-2) equations:
    //  Table[a[i] points[[i + 1, 1]]^2 + b[i] points[[i + 1, 1]] + c[i] == points[[i + 1, 2]], {i, n - 2}]
    for(int i=0; i<n-2; ++i)
    {
        /*a[i]*/M(2+i, 3*i+A)=sqr(points[i+1].x);
        /*b[i]*/M(2+i, 3*i+B)=    points[i+1].x ;
        /*c[i]*/M(2+i, 3*i+C)=1;
        /*RHS*/ R(2+i)=points[i+1].y;
    }

    // Value of ith function at midpoint between points (i + 1) and (i + 2) must agree with that of (i + 1)th function
    // This gives (n-3) equations:
    //  Table[a[i] ((points[[i + 1, 1]] + points[[i + 2, 1]])/2)^2 + b[i] (points[[i + 1, 1]] + points[[i + 2, 1]])/2 + c[i] == 
    //          a[i + 1] ((points[[i + 1, 1]] + points[[i + 2, 1]])/2)^2 + b[i + 1] (points[[i + 1, 1]] + points[[i + 2, 1]])/2 + c[i + 1]
    //        , {i, n - 3}]
    for(int i=0; i<n-3; ++i)
    {
        /*a[i]*/  M(n+i, 3*i+A)     =  sqr(0.5*(points[i+1].x+points[i+2].x));
        /*b[i]*/  M(n+i, 3*i+B)     =      0.5*(points[i+1].x+points[i+2].x) ;
        /*c[i]*/  M(n+i, 3*i+C)     =  1;
        /*a[i+1]*/M(n+i, 3*(i+1)+A) = -sqr(0.5*(points[i+1].x+points[i+2].x));
        /*b[i+1]*/M(n+i, 3*(i+1)+B) = -    0.5*(points[i+1].x+points[i+2].x) ;
        /*c[i+1]*/M(n+i, 3*(i+1)+C) = -1;
    }

    // Same for derivatives at midpoints, giving us another (n-3) equations:
    //  Table[2 a[i] (points[[i + 1, 1]] + points[[i + 2, 1]])/2 + b[i] == 2 a[i + 1] (points[[i + 1, 1]] + points[[i + 2, 1]])/2 + b[i + 1]
    //        , {i, n - 3}]
    for(int i=0; i<n-3; ++i)
    {
        /*a[i]*/  M(2*n-3+i, 3*i+A)     =  points[i+1].x+points[i+2].x;
        /*b[i]*/  M(2*n-3+i, 3*i+B)     =  1;
        /*a[i+1]*/M(2*n-3+i, 3*(i+1)+A) = -(points[i+1].x+points[i+2].x);
        /*b[i+1]*/M(2*n-3+i, 3*(i+1)+B) = -1;
    }

    const Vector ABCs = M.colPivHouseholderQr().solve(R);

    std::vector<typename SplineOrder2InterpolationFunction<Number,Vec2>::Chunk> coefs;

    // Left endpoint
    coefs.emplace_back((points[1].x+points[2].x)/2,
                       ABCs(A), ABCs(B), ABCs(C));

    // Internal points
    for(int i=1; i<n-3; ++i)
        coefs.emplace_back((points[i+1].x+points[i+2].x)/2,
                           ABCs(3*i+A), ABCs(3*i+B), ABCs(3*i+C));

    // Right endpoint
    coefs.emplace_back(points[n-1].x,
                       ABCs(3*(n-3)+A), ABCs(3*(n-3)+B), ABCs(3*(n-3)+C));

    return coefs;
}

#endif
