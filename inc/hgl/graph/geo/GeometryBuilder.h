#pragma once

#include<hgl/graph/GeometryCreater.h>
#include<hgl/graph/VertexAttribDataAccess.h>

namespace hgl::graph::inline_geometry
{
    /**
     * 几何体构建器基类
     * 封装VAB映射的初始化和管理，提供统一的顶点写入接口
     */
    class GeometryBuilder
    {
    protected:
        GeometryCreater *creater;
        
        // VAB映射指针
        float *vp;      // 顶点位置指针
        float *np;      // 法线指针
        float *tp;      // 切线指针
        float *tcp;     // 纹理坐标指针

    public:
        GeometryBuilder(GeometryCreater *pc);
        virtual ~GeometryBuilder() = default;

        /**
         * 检查构建器是否有效
         * @return 如果顶点位置指针有效返回true
         */
        bool IsValid() const { return vp != nullptr; }

        /**
         * 写入顶点位置
         * @param x, y, z 顶点坐标
         */
        inline void WriteVertex(float x, float y, float z)
        {
            if(vp)
            {
                *vp++ = x;
                *vp++ = y;
                *vp++ = z;
            }
        }

        /**
         * 写入法线
         * @param x, y, z 法线坐标
         */
        inline void WriteNormal(float x, float y, float z)
        {
            if(np)
            {
                *np++ = x;
                *np++ = y;
                *np++ = z;
            }
        }

        /**
         * 写入切线
         * @param x, y, z 切线坐标
         */
        inline void WriteTangent(float x, float y, float z)
        {
            if(tp)
            {
                *tp++ = x;
                *tp++ = y;
                *tp++ = z;
            }
        }

        /**
         * 写入纹理坐标
         * @param u, v 纹理坐标
         */
        inline void WriteTexCoord(float u, float v)
        {
            if(tcp)
            {
                *tcp++ = u;
                *tcp++ = v;
            }
        }

        /**
         * 写入完整顶点数据（位置+法线+切线+纹理坐标）
         * @param px, py, pz 顶点位置
         * @param nx, ny, nz 法线
         * @param tx, ty, tz 切线
         * @param u, v 纹理坐标
         */
        inline void WriteFullVertex(float px, float py, float pz,
                                   float nx, float ny, float nz,
                                   float tx, float ty, float tz,
                                   float u, float v)
        {
            WriteVertex(px, py, pz);
            WriteNormal(nx, ny, nz);
            WriteTangent(tx, ty, tz);
            WriteTexCoord(u, v);
        }

        /**
         * 检查是否有法线缓冲
         */
        bool HasNormals() const { return np != nullptr; }

        /**
         * 检查是否有切线缓冲
         */
        bool HasTangents() const { return tp != nullptr; }

        /**
         * 检查是否有纹理坐标缓冲
         */
        bool HasTexCoords() const { return tcp != nullptr; }
    };
}
