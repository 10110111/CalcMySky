/*
 * CalcMySky - a simulator of light scattering in planetary atmospheres
 * Copyright © 2026 Ruslan Kabatsayev
 * This file is based on Mesh Simplification Tutorial by Sven Forstmann.
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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <math.h>
#include <algorithm>

#define loopi(start_l,end_l) for ( int i=start_l;i<int(end_l);++i )
#define loopj(start_l,end_l) for ( int j=start_l;j<int(end_l);++j )
#define loopk(start_l,end_l) for ( int k=start_l;k<int(end_l);++k )

struct vec3f
{
    double x, y, z;

    inline vec3f() = default;

    inline vec3f( const vec3f& a )
     { x = a.x; y = a.y; z = a.z; }

    inline vec3f( const double X, const double Y, const double Z )
    { x = X; y = Y; z = Z; }

    inline vec3f operator + ( const vec3f& a ) const
    { return vec3f( x + a.x, y + a.y, z + a.z ); }

    inline vec3f operator += ( const vec3f& a ) const
    { return vec3f( x + a.x, y + a.y, z + a.z ); }

    inline vec3f operator * ( const double a ) const
    { return vec3f( x * a, y * a, z * a ); }

    inline vec3f operator * ( const vec3f a ) const
    { return vec3f( x * a.x, y * a.y, z * a.z ); }

    inline vec3f operator = ( const vec3f& a )
    { x=a.x;y=a.y;z=a.z;return *this; }

    inline vec3f operator / ( const vec3f& a ) const
    { return vec3f( x / a.x, y / a.y, z / a.z ); }

    inline vec3f operator - ( const vec3f& a ) const
    { return vec3f( x - a.x, y - a.y, z - a.z ); }

    inline vec3f operator / ( const double a ) const
    { return vec3f( x / a, y / a, z / a ); }

    inline double dot( const vec3f& a ) const
    { return a.x*x + a.y*y + a.z*z; }

    inline vec3f cross( const vec3f& a , const vec3f& b )
    {
        x = a.y * b.z - a.z * b.y;
        y = a.z * b.x - a.x * b.z;
        z = a.x * b.y - a.y * b.x;
        return *this;
    }

    inline vec3f normalize()
    {
        double square = sqrt(x*x + y*y + z*z);
        x/=square;y/=square;z/=square;

        return *this;
    }
};

class SymmetricMatrix {

    public:

    // Constructor

    SymmetricMatrix(double c=0) { loopi(0,10) m[i] = c;  }

    SymmetricMatrix(    double m11, double m12, double m13, double m14,
                        double m22, double m23, double m24,
                                    double m33, double m34,
                                                double m44) {
             m[0] = m11;  m[1] = m12;  m[2] = m13;  m[3] = m14;
                          m[4] = m22;  m[5] = m23;  m[6] = m24;
                                       m[7] = m33;  m[8] = m34;
                                                    m[9] = m44;
    }

    // Make plane

    SymmetricMatrix(double a,double b,double c,double d)
    {
        m[0] = a*a;  m[1] = a*b;  m[2] = a*c;  m[3] = a*d;
                     m[4] = b*b;  m[5] = b*c;  m[6] = b*d;
                                  m[7 ] =c*c; m[8 ] = c*d;
                                               m[9 ] = d*d;
    }

    double operator[](int c) const { return m[c]; }

    // Determinant

    double det(    int a11, int a12, int a13,
                int a21, int a22, int a23,
                int a31, int a32, int a33)
    {
        double det =  m[a11]*m[a22]*m[a33] + m[a13]*m[a21]*m[a32] + m[a12]*m[a23]*m[a31]
                    - m[a13]*m[a22]*m[a31] - m[a11]*m[a23]*m[a32]- m[a12]*m[a21]*m[a33];
        return det;
    }

    const SymmetricMatrix operator+(const SymmetricMatrix& n) const
    {
        return SymmetricMatrix( m[0]+n[0],   m[1]+n[1],   m[2]+n[2],   m[3]+n[3],
                                            m[4]+n[4],   m[5]+n[5],   m[6]+n[6],
                                                         m[ 7]+n[ 7], m[ 8]+n[8 ],
                                                                      m[ 9]+n[9 ]);
    }

    SymmetricMatrix& operator+=(const SymmetricMatrix& n)
    {
         m[0]+=n[0];   m[1]+=n[1];   m[2]+=n[2];   m[3]+=n[3];
         m[4]+=n[4];   m[5]+=n[5];   m[6]+=n[6];   m[7]+=n[7];
         m[8]+=n[8];   m[9]+=n[9];
        return *this;
    }

    double m[10];
};
///////////////////////////////////////////

namespace Simplify
{
    // Global Variables & Strctures
    struct Triangle { int v[3];double err[4];int8_t deleted,dirty;vec3f n; };
    struct Vertex { vec3f p;int tstart,tcount;SymmetricMatrix q;int8_t border;};
    struct Ref { int tid,tvertex; };
    std::vector<std::vector<Triangle> *> triangles;
    std::vector<std::vector<Vertex> *> vertices;
    std::vector<std::vector<Ref> *> refs;
    double xMin, xMax;

    // Helper functions
    double vertex_error(SymmetricMatrix q, double x, double y, double z);
    double calculate_error(int id_v1, int id_v2, bool move_by_quadric, vec3f &p_result, int thread);
    bool flipped(vec3f p,int i1,const Vertex &v0,std::vector<int> &deleted,int thread);
    void update_triangles(int i0,const Vertex &v, bool move_by_quadric,const std::vector<int> &deleted,int &deleted_triangles, int thread);
    void update_mesh(int iteration, bool move_by_quadric, int thread);
    void compact_mesh(int thread);

    void allocate(int numThreads){
        for(int i = 0; i < numThreads; i++){
            triangles.push_back(new std::vector<Triangle>());
            vertices.push_back(new std::vector<Vertex>());
            refs.push_back(new std::vector<Ref>());
        }
    }

    void cleanup(){
        for (auto &t : triangles) delete t;
        for (auto &v : vertices) delete v;
        for (auto &r : refs) delete r;
    }

    //
    // Main simplification function
    //
    // target_count  : target nr. of triangles
    // agressiveness : sharpness to increase the threashold.
    //                 5..8 are good numbers
    //                 more iterations yield higher quality
    //

    void simplify_mesh(int target_count, double agressiveness, double xMin, double xMax,
                       bool move_by_quadric, bool verbose, int thread)
    {
        Simplify::xMin = xMin;
        Simplify::xMax = xMax;
        // main iteration loop
        int deleted_triangles=0;
        std::vector<int> deleted0,deleted1;
        int triangle_count=triangles[thread]->size();
        //int iteration = 0;
        //loop(iteration,0,100)
        for (int iteration = 0; iteration < 1000; iteration ++)
        {
            if(triangle_count-deleted_triangles<=target_count)break;
            //
            // All triangles with edges below the threshold will be removed
            //
            // The following numbers works well for most models.
            // If it does not, try to adjust the 3 parameters
            //
            double threshold = 0.000000001*pow(double(iteration+3),agressiveness);

            // update mesh once in a while
            if(iteration%5==0)
            {
                update_mesh(iteration, move_by_quadric, thread);
            }

            // clear dirty flag
            loopi(0,triangles[thread]->size()) (*triangles[thread])[i].dirty=0;

            // target number of triangles reached ? Then break
            if ((verbose) && (iteration%5==0)) {
                printf("iteration %d - triangles %d threshold %g\n",iteration,triangle_count-deleted_triangles, threshold);
            }

            // remove vertices & mark deleted triangles
            loopi(0,triangles[thread]->size())
            {
                Triangle &t=(*triangles[thread])[i];
                if(t.err[3]>threshold) continue;
                if(t.deleted == 1) continue;
                if(t.deleted == -1) continue;
                if(t.dirty) continue;

                loopj(0,3)if(t.err[j]<threshold)
                {

                    int i0=t.v[ j     ]; Vertex &v0 = (*vertices[thread])[i0];
                    int i1=t.v[(j+1)%3]; Vertex &v1 = (*vertices[thread])[i1];
                    // Border check
                    if(v0.border != v1.border)  continue;

                    // Compute vertex to collapse to
                    vec3f p;
                    calculate_error(i0,i1,move_by_quadric,p,thread);
                    deleted0.resize(v0.tcount); // normals temporarily
                    deleted1.resize(v1.tcount); // normals temporarily
                    // dont remove if flipped
                    if( flipped(p,i1,v0,deleted0,thread) ) continue;

                    if( flipped(p,i0,v1,deleted1,thread) ) continue;

                    // not flipped, so remove edge
                    v0.p=p;
                    v0.q=v1.q+v0.q;
                    int tstart=refs[thread]->size();

                    update_triangles(i0,v0,move_by_quadric,deleted0,deleted_triangles,thread);
                    update_triangles(i0,v1,move_by_quadric,deleted1,deleted_triangles,thread);

                    int tcount=refs[thread]->size()-tstart;

                    if(tcount<=v0.tcount)
                    {
                        // save ram
                        if(tcount)memcpy(&(*refs[thread])[v0.tstart],&(*refs[thread])[tstart],tcount*sizeof(Ref));
                    }
                    else
                        // append
                        v0.tstart=tstart;

                    v0.tcount=tcount;
                    break;
                }
                // done?
                if(triangle_count-deleted_triangles<=target_count)break;
            }
        }
        // clean up mesh
        compact_mesh(thread);
    } //simplify_mesh()

    // Check if a triangle flips when this edge is removed

    bool flipped(vec3f p,int i1,const Vertex &v0,std::vector<int> &deleted, int thread)
    {

        loopk(0,v0.tcount)
        {
            const Triangle &t=(*triangles[thread])[(*refs[thread])[v0.tstart+k].tid];
            if(t.deleted == 1)continue;

            int s=(*refs[thread])[v0.tstart+k].tvertex;
            int id1=t.v[(s+1)%3];
            int id2=t.v[(s+2)%3];

            if(id1==i1 || id2==i1) // delete ?
            {

                deleted[k]=1;
                continue;
            }
            vec3f d1 = (*vertices[thread])[id1].p-p; d1.normalize();
            vec3f d2 = (*vertices[thread])[id2].p-p; d2.normalize();
            vec3f proj1(d1.x, d1.y, 0), proj2(d2.x, d2.y, 0);
            proj1.normalize();
            proj2.normalize();
            if(fabs(proj1.dot(proj2))>0.999) return true;
            vec3f n;
            n.cross(d1,d2);
            n.normalize();
            deleted[k]=0;
            if(n.dot(t.n)<0.2) return true;
        }
        return false;
    }

    // Update triangle connections and edge error after a edge is collapsed

    void update_triangles(int i0,const Vertex &v,bool move_by_quadric,const std::vector<int> &deleted,int &deleted_triangles, int thread)
    {
        vec3f p;
        loopk(0,v.tcount)
        {
            const Ref &r=(*refs[thread])[v.tstart+k];
            Triangle &t=(*triangles[thread])[r.tid];
            if(t.deleted == 1)continue;

            if(deleted[k])
            {
                t.deleted=1;
                deleted_triangles++;
                continue;
            }
            t.v[r.tvertex]=i0;
            t.dirty=1;
            t.err[0]=calculate_error(t.v[0],t.v[1],move_by_quadric,p,thread);
            t.err[1]=calculate_error(t.v[1],t.v[2],move_by_quadric,p,thread);
            t.err[2]=calculate_error(t.v[2],t.v[0],move_by_quadric,p,thread);
            t.err[3]=std::min({t.err[0],t.err[1],t.err[2]});
            refs[thread]->push_back(r);
        }
    }

    // compact triangles, compute edge error and build reference list

    void update_mesh(int iteration, bool move_by_quadric, int thread)
    {
        auto& tris = *triangles[thread];
        if(iteration>0) // compact triangles
        {
            int dst=0;
            loopi(0,tris.size())
            if(tris[i].deleted == 0 || tris[i].deleted == -1)
            {
                tris[dst++]=tris[i];
            }
            tris.resize(dst);
        }
        //
        // Init Quadrics by Plane & Edge Errors
        //
        // required at the beginning ( iteration == 0 )
        // recomputing during the simplification is not required,
        // but mostly improves the result for closed meshes
        //
        auto& verts = *vertices[thread];
        if( iteration == 0 )
        {
            loopi(0,verts.size())
                verts[i].q=SymmetricMatrix(0.0);

            loopi(0,tris.size())
            {
                Triangle &t=tris[i];
                vec3f n,p[3];
                loopj(0,3) p[j]=verts[t.v[j]].p;
                n.cross(p[1]-p[0],p[2]-p[0]);
                n.normalize();
                t.n=n;
                loopj(0,3) verts[t.v[j]].q =
                    verts[t.v[j]].q+SymmetricMatrix(n.x,n.y,n.z,-n.dot(p[0]));
            }
            loopi(0,tris.size())
            {
                // Calc Edge Error
                Triangle &t=tris[i];vec3f p;
                loopj(0,3) t.err[j]=calculate_error(t.v[j],t.v[(j+1)%3],move_by_quadric,p,thread);
                t.err[3]=std::min({t.err[0],t.err[1],t.err[2]});
            }
        }

        // Init Reference ID list
        loopi(0,verts.size())
        {
            verts[i].tstart=0;
            verts[i].tcount=0;
        }
        loopi(0,tris.size())
        {
            Triangle &t=tris[i];
            loopj(0,3) verts[t.v[j]].tcount++;
        }
        int tstart=0;
        loopi(0,verts.size())
        {
            Vertex &v=verts[i];
            v.tstart=tstart;
            tstart+=v.tcount;
            v.tcount=0;
        }

        // Write References
        refs[thread]->resize(tris.size()*3);
        loopi(0,tris.size())
        {
            Triangle &t=tris[i];
            loopj(0,3)
            {
                Vertex &v=verts[t.v[j]];
                (*refs[thread])[v.tstart+v.tcount].tid=i;
                (*refs[thread])[v.tstart+v.tcount].tvertex=j;
                v.tcount++;
            }
        }

        // Identify boundary : vertices[].border=0,1
        if( iteration == 0 )
        {
            std::vector<unsigned> vcount,vids;

            loopi(0,verts.size())
                verts[i].border=0;

            loopi(0,verts.size())
            {
                Vertex &v=verts[i];
                vcount.clear();
                vids.clear();
                loopj(0,v.tcount)
                {
                    int k=(*refs[thread])[v.tstart+j].tid;
                    Triangle &t=tris[k];
                    loopk(0,3)
                    {
                        unsigned ofs=0,id=t.v[k];
                        while(ofs<vcount.size())
                        {
                            if(vids[ofs]==id)break;
                            ofs++;
                        }
                        if(ofs==vcount.size())
                        {
                            vcount.push_back(1);
                            vids.push_back(id);
                        }
                        else
                            vcount[ofs]++;
                    }
                }
                loopj(0,vcount.size()) if(vcount[j]==1)
                    verts[vids[j]].border=1;
            }
        }
    }

    // Finally compact mesh before exiting

    void compact_mesh(int thread)
    {
        int dst=0;
        loopi(0,vertices[thread]->size())
        {
            (*vertices[thread])[i].tcount=0;
        }
        loopi(0,triangles[thread]->size())
        if((*triangles[thread])[i].deleted == 0 || (*triangles[thread])[i].deleted == -1)
        {
            Triangle &t=(*triangles[thread])[i];
            (*triangles[thread])[dst++]=t;
            loopj(0,3)(*vertices[thread])[t.v[j]].tcount=1;
        }
        triangles[thread]->resize(dst);
        dst=0;
        loopi(0,vertices[thread]->size())
        if((*vertices[thread])[i].tcount)
        {
            (*vertices[thread])[i].tstart=dst;
            (*vertices[thread])[dst].p=(*vertices[thread])[i].p;
            dst++;
        }
        loopi(0,triangles[thread]->size())
        {
            Triangle &t=(*triangles[thread])[i];
            loopj(0,3)t.v[j]=(*vertices[thread])[t.v[j]].tstart;
        }
        vertices[thread]->resize(dst);
    }

    // Error between vertex and Quadric

    double vertex_error(SymmetricMatrix q, double x, double y, double z)
    {
         double e = q[0]*x*x + 2*q[1]*x*y + 2*q[2]*x*z + 2*q[3]*x + q[4]*y*y
                  + 2*q[5]*y*z + 2*q[6]*y + q[7]*z*z + 2*q[8]*z + q[9];
         // Turn it to a relative error
         e /= z;
         // Increase on the sides
         const double s = (x - xMin) / (xMax - xMin);
         e *= 1 + 10 * (std::exp(-100 * s) + std::exp(100 * (s - 1)));
         return e;
    }

    // Error for one edge

    double calculate_error(int id_v1, int id_v2, bool move_by_quadric, vec3f &p_result, int thread)
    {
        // compute interpolated vertex

        const auto& v1 = (*vertices[thread])[id_v1];
        const auto& v2 = (*vertices[thread])[id_v2];
        SymmetricMatrix q = v1.q + v2.q;
        bool   border = v1.border & v2.border;
        double error=0;
        double det = q.det(0, 1, 2, 1, 4, 5, 2, 5, 7);
        if ( move_by_quadric && det != 0 && !border )
        {

            // q_delta is invertible
            p_result.x = -1/det*(q.det(1, 2, 3, 4, 5, 6, 5, 7 , 8));    // vx = A41/det(q_delta)
            p_result.y =  1/det*(q.det(0, 2, 3, 1, 5, 6, 2, 7 , 8));    // vy = A42/det(q_delta)
            p_result.z = -1/det*(q.det(0, 1, 3, 1, 4, 6, 2, 5,  8));    // vz = A43/det(q_delta)

            error = vertex_error(q, p_result.x, p_result.y, p_result.z);
        }
        else
        {
            // det = 0 -> try to find best result
            vec3f p1=v1.p;
            vec3f p2=v2.p;
            vec3f p3=(p1*0.25+p2*0.75);
            vec3f p4=(p1+p2)/2;
            vec3f p5=(p1*0.75+p2*0.25);
            double error1 = vertex_error(q, p1.x,p1.y,p1.z);
            double error2 = vertex_error(q, p2.x,p2.y,p2.z);
            double error3 = vertex_error(q, p3.x,p3.y,p3.z);
            double error4 = vertex_error(q, p4.x,p4.y,p4.z);
            double error5 = vertex_error(q, p5.x,p5.y,p5.z);
            error = std::min({error1, error2, error3, error4, error5});
            if (error1 == error) p_result=p1;
            if (error2 == error) p_result=p2;
            if (error3 == error) p_result=p3;
            if (error4 == error) p_result=p4;
            if (error5 == error) p_result=p5;
        }
        return error;
    }
};
///////////////////////////////////////////
