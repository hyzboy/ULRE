#ifndef HGL_ALGORITHM_MATH_VECTOR_MATRIX_INCLUDE
#define HGL_ALGORITHM_MATH_VECTOR_MATRIX_INCLUDE

#include<hgl/math/Vector.h>
#include<hgl/TypeFunc.h>
//注：GLM/CML(OpenGLMode)是列矩阵,计算坐标matrix*pos
//   而MGL是行矩阵，需要反过来pos*matrix

namespace hgl
{
    using Matrix3f=float3x3;
    using Matrix4f=float4x4;

    struct WorldMatrix
    {
        alignas(16) Matrix4f ortho;         //2D正角视图矩阵

        alignas(16) Matrix4f projection;
//        alignas(16) Matrix4f inverse_projection;
        alignas(16) Matrix4f modelview;
        alignas(16) Matrix4f mvp;
        alignas(16) Vector4f view_pos;      //眼睛坐标
    };//struct WorldMatrix

    inline Matrix4f identity()
    {
        return Matrix4f::identity;
    }

    inline Matrix4f inverse(const Matrix4f &m)
    {
        return m.Inverted();
    }

    inline Matrix4f ortho(  float left,
                            float right,
                            float bottom,
                            float top,
                            float znear,
                            float zfar )
    {
        return Matrix4f(
            2.0f / (right - left),  0.0f,                   0.0f,                   -(right + left) / (right - left),
            0.0f,                   2.0f / (bottom - top),  0.0f,                   -(bottom + top) / (bottom - top),
            0.0f,                   0.0f,                   1.0f / (znear - zfar),  znear / (znear - zfar),
            0.0f,                   0.0f,                   0.0f,                   1.0f);
    }

    /**
     * 生成一个正角视图矩阵
     * @param width 宽
     * @param height 高
     * @param znear 近平面z值
     * @param zfar 远平台z值
     */
    inline Matrix4f ortho(float width,float height,float znear,float zfar)
    {
        return Matrix4f(
            2.0f / width,   0.0f,           0.0f,                   -1,
            0.0f,           2.0f / height,  0.0f,                   -1,
            0.0f,           0.0f,           1.0f / (znear - zfar),  znear / (znear - zfar),
            0.0f,           0.0f,           0.0f,                   1.0f);
    }

    /**
     * 生成一个正角视图矩阵
     * @param width 宽
     * @param height 高
     */
    inline Matrix4f ortho(float width,float height)
    {
        return Matrix4f(
            2.0f / width,   0.0f,           0.0f,   -1,
            0.0f,           2.0f / height,  0.0f,   -1,
            0.0f,           0.0f,           -1.0f , 0.0f,
            0.0f,           0.0f,           0.0f,   1.0f);
    }

    /**
     * 生成一个透视矩阵
     * @param aspect_ratio 宽高比
     * @param field_of_view 视野
     * @param znear 近截面
     * @param zfar 远截面
     */
    inline Matrix4f perspective(float field_of_view,
                                float aspect_ratio,                                
                                float znear,
                                float zfar)
    {
        const float f = 1.0f / tan( hgl_ang2rad( 0.5f * field_of_view ) );

  //      float  scaleX, shearXy, shearXz, x;
		//float shearYx,  scaleY, shearYz, y;
		//float shearZx, shearZy,  scaleZ, z;
		//float shearWx, shearWy, shearWz, w;

        return Matrix4f(
            f / aspect_ratio,   0.0f,   0.0f,                   0.0f,
            0.0f,               -f,     0.0f,                   0.0f,
            0.0f,               0.0f,   zfar / (znear - zfar),  (znear * zfar) / (znear - zfar),
                                    //  ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
                                    //  某些引擎这两项会乘0.5，那是因为他们是 -1 to 1 的Z值设定，而我们是0 to 1，所以这里不用乘
                                    //  同理，camera的znear为接近0的正数，zfar为一个较大的正数，默认使用16/256

            0.0f,               0.0f,   -1.0f,                  0.0f);
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
}//namespace hgl
#endif//HGL_ALGORITHM_MATH_VECTOR_MATRIX_INCLUDE
