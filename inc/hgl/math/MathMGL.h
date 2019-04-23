#ifndef HGL_ALGORITHM_VECTOR_MATH_MGL_INCLUDE
#define HGL_ALGORITHM_VECTOR_MATH_MGL_INCLUDE

#ifdef _MSC_VER
#pragma warning(disable:4244)           // double -> int 精度丢失警告
#endif//_MSC_VER

#include<MathGeoLib.h>

/**
 * MathGeoLib
 * Game Math and Geometry Library
 *
 * My C++ library for 3D mathematics and geometry manipulation.
 * Jukka Jylänki
 *
 * offical web: http://clb.demon.fi/MathGeoLib/nightly/
 *
 * License:
 *
 *      This library is licensed under the Apache 2 license. I am not a lawyer, but to me that
 * license means that you can use this code for any purpose, both commercial and closed source.
 * You are however restricted from claiming you wrote it yourself, and cannot hold me liable
 * for anything over this code.
 *      I acknowledge that most of the non-trivial math routines are taken off a book or a
 * research paper. In all places, I have tried to be diligent to properly attribute the original
 * source. Please contact me if you feel I have misattributed something.
 */

namespace hgl
{
    using namespace math;

    typedef float2 Vector2f;
    typedef float3 Vector3f;
    typedef float4 Vector4f;

    typedef float3x3 Matrix3f;
    typedef float4x4 Matrix4f;

    inline bool operator == (const Vector2f &lhs,const Vector2f &rhs)
    {
        if(lhs.x!=rhs.x)return(false);
        if(lhs.y!=rhs.y)return(false);
        return(true);
    }

    inline bool operator != (const Vector2f &lhs,const Vector2f &rhs)
    {
        if(lhs.x!=rhs.x)return(true);
        if(lhs.y!=rhs.y)return(true);
        return(false);
    }

    inline bool operator == (const Vector3f &lhs,const Vector3f &rhs)
    {
        if(lhs.x!=rhs.x)return(false);
        if(lhs.y!=rhs.y)return(false);
        if(lhs.z!=rhs.z)return(false);
        return(true);
    }

    inline bool operator != (const Vector3f &lhs,const Vector3f &rhs)
    {
        if(lhs.x!=rhs.x)return(true);
        if(lhs.y!=rhs.y)return(true);
        if(lhs.z!=rhs.z)return(true);
        return(false);
    }

    inline bool operator == (const Vector4f &lhs,const Vector4f &rhs)
    {
        if(lhs.x!=rhs.x)return(false);
        if(lhs.y!=rhs.y)return(false);
        if(lhs.z!=rhs.z)return(false);
        if(lhs.w!=rhs.w)return(false);
        return(true);
    }

    inline bool operator != (const Vector4f &lhs,const Vector4f &rhs)
    {
        if(lhs.x!=rhs.x)return(true);
        if(lhs.y!=rhs.y)return(true);
        if(lhs.z!=rhs.z)return(true);
        if(lhs.w!=rhs.w)return(true);
        return(false);
    }

    inline void vec3to2(Vector2f &dst,const Vector3f &src)
    {
        dst.x=src.x;
        dst.y=src.y;
    }

    inline Vector2f vec3to2(const Vector3f &src)
    {
        return Vector2f(src.x,src.y);
    }

    inline void vec2to3(Vector3f &dst,const Vector2f &src,const float z)
    {
        dst.x=src.x;
        dst.y=src.y;
        dst.z=z;
    }

    inline Vector3f vec2to3(const Vector2f &src,const float z)
    {
        return Vector3f(src.x,src.y,z);
    }

    inline Matrix4f identity()
    {
        return Matrix4f::identity;
    }

    inline Matrix4f inverse(const Matrix4f &m)
    {
        return m.Inverted();
    }

    inline Matrix4f ortho(  float left_plane,
                            float right_plane,
                            float bottom_plane,
                            float top_plane,
                            float near_plane,
                            float far_plane ) 
    {
        Matrix4f orthographic_projection_matrix = 
        {
            2.0f / (right_plane - left_plane),0.0f,0.0f,-(right_plane + left_plane) / (right_plane - left_plane),
            0.0f,2.0f / (bottom_plane - top_plane),0.0f,-(bottom_plane + top_plane) / (bottom_plane - top_plane),
            0.0f,0.0f,1.0f / (near_plane - far_plane),near_plane / (near_plane - far_plane),
            0.0f,0.0f,0.0f,1.0f
        };

        return orthographic_projection_matrix;
    }
    /**
     * 生成一个2D正角视图矩阵
     * @param width 宽
     * @param height 高
     * @param znear 近平面z值
     * @param zfar 远平台z值
     */
    inline Matrix4f ortho(float width,float height,float znear=0,float zfar=1)
    {
        Matrix4f orthographic_projection_matrix = 
        {
            2.0f / width,   0.0f,           0.0f,                   -1,
            0.0f,           2.0f / height,  0.0f,                   -1,
            0.0f,           0.0f,           1.0f / (znear - zfar),  znear / (znear - zfar),
            0.0f,           0.0f,           0.0f,                   1.0f
        };

        return orthographic_projection_matrix;
    }

    inline Matrix4f perspective(float aspect_ratio,
                                float field_of_view,
                                float near_plane,
                                float far_plane ) 
    {
        const float f = 1.0f / tan( hgl_ang2rad( 0.5f * field_of_view ) );

        Matrix4f perspective_projection_matrix = 
        {
            f / aspect_ratio,   0.0f,   0.0f,                                   0.0f,
            0.0f,               -f,     0.0f,                                   0.0f,
            0.0f,               0.0f,   far_plane / (near_plane - far_plane),   (near_plane * far_plane) / (near_plane - far_plane),
            0.0f,               0.0f,   -1.0f,                                  0.0f
        };

        return perspective_projection_matrix;
    }

    inline Matrix4f translate(const Vector3f &v)
    {
        return Matrix4f::Translate(v);
    }

    inline Matrix4f translate(float x,float y,float z)
    {
        return Matrix4f::Translate(x,y,z);
    }

    inline Matrix4f scale(const Vector3f &v)
    {
        return Matrix4f::Scale(v,Vector3f::zero);
    }

    inline Matrix4f scale(float x,float y,float z)
    {
        return Matrix4f::Scale(Vector3f(x,y,z),Vector3f::zero);
    }

    inline Matrix4f scale(float s)
    {
        return Matrix4f::Scale(Vector3f(s,s,s),Vector3f::zero);
    }

    inline Matrix4f rotate(float angle,const Vector3f &axis)
    {
        return Matrix4f::RotateAxisAngle(axis.Normalized(),angle);
    }

    inline Matrix4f rotate(float angle,float x,float y,float z)
    {
        return rotate(angle,Vector3f(x,y,z));
    }

    inline Matrix4f rotate(float angle,const Vector4f &axis)
    {
        return rotate(angle,Vector3f(axis.x,axis.y,axis.z));
    }

    inline Vector3f rotate(const Vector3f &v3f,float angle,const Vector3f &axis)
    {
        Vector4f result=rotate(angle,axis)*Vector4f(v3f,1.0f);

        return result.xyz();
    }

    template<typename T>
    inline T normalized(const T &v)
    {
        return v.Normalized();
    }

    template<typename T>
    inline void normalize(T &v)
    {
        v.Normalize();
    }

    template<typename T>
    inline T cross(const T &v1,const T &v2)
    {
        return v1.Cross(v2);
    }

    template<typename T>
    inline float dot(const T &v1,const T &v2)
    {
        return v1.Dot(v2);
    }

    inline float ray_angle_cos(const Ray &ray,const vec &pos)
    {
        return ray.dir.Dot((pos-ray.pos).Normalized());
    }
}//namespace hgl
#endif//HGL_ALGORITHM_VECTOR_MATH_MGL_INCLUDE
