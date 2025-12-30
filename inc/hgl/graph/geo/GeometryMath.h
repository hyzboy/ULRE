#pragma once

#include<hgl/math/Math.h>
#include<hgl/math/Vector.h>

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    /**
     * 几何体数学工具库
     * 提供常用的数学计算函数，避免重复代码
     */
    namespace GeometryMath
    {
        /**
         * 归一化2D向量
         * @param v 输入向量
         * @return 归一化后的向量，如果长度太小则返回零向量
         */
        Vector2f Normalize(const Vector2f &v);

        /**
         * 归一化3D向量
         * @param v 输入向量
         * @return 归一化后的向量，如果长度太小则返回零向量
         */
        Vector3f Normalize(const Vector3f &v);

        /**
         * 计算三角形法线
         * @param A 三角形第一个顶点
         * @param B 三角形第二个顶点
         * @param C 三角形第三个顶点
         * @return 三角形法线向量（未归一化）
         */
        Vector3f TriangleNormal(const Vector3f &A, const Vector3f &B, const Vector3f &C);

        /**
         * 计算两条2D直线的交点
         * @param p 第一条直线上的一点
         * @param r 第一条直线的方向向量
         * @param q 第二条直线上的一点
         * @param s 第二条直线的方向向量
         * @param t 输出参数：第一条直线上的参数 (p + t*r)
         * @param u 输出参数：第二条直线上的参数 (q + u*s)
         * @return 如果直线相交返回true，否则返回false（平行或重合）
         */
        bool LineIntersect(const Vector2f &p, const Vector2f &r, 
                          const Vector2f &q, const Vector2f &s, 
                          float &t, float &u);

        /**
         * 计算两个2D向量之间的夹角（度数）
         * @param ax 第一个向量的x分量
         * @param ay 第一个向量的y分量
         * @param bx 第二个向量的x分量
         * @param by 第二个向量的y分量
         * @return 夹角（0-180度）
         */
        float AngleBetween(const float ax, const float ay, const float bx, const float by);

        /**
         * 创建绕Y轴旋转的四元数
         * @param quaternion 输出的四元数 [x,y,z,w]
         * @param angle 旋转角度（度数）
         */
        void QuaternionRotateY(float quaternion[4], const float angle);

        /**
         * 创建绕Z轴旋转的四元数
         * @param quaternion 输出的四元数 [x,y,z,w]
         * @param angle 旋转角度（度数）
         */
        void QuaternionRotateZ(float quaternion[4], const float angle);

        /**
         * 将四元数转换为4x4矩阵
         * @param matrix 输出的16个float组成的矩阵（列主序）
         * @param quaternion 输入的四元数 [x,y,z,w]
         */
        void QuaternionToMatrix(float matrix[16], const float quaternion[4]);

        /**
         * 用4x4矩阵乘以3D向量
         * @param result 输出的结果向量
         * @param matrix 16个float组成的矩阵（列主序）
         * @param vector 输入的3D向量
         */
        void MatrixMultiplyVector3(float result[3], const float matrix[16], const float vector[3]);
    }
}
