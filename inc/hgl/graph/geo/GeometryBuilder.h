#pragma once

#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/geo/FormatAwareWriter.h>
#include<hgl/vk/VKBufferAccessor.h>

namespace hgl::graph::inline_geometry
{
    /**
     * 几何体构建器基类
     * 使用 BufferAccessor 封装顶点属性访问，提供统一的顶点写入接口
     */
    class GeometryBuilder
    {
    protected:
        GeometryCreater *creater;

        BufferAccessor3f accessor_position;
        BufferAccessor3f accessor_normal;
        BufferAccessor3f accessor_tangent;
        BufferAccessor2sn8 accessor_normal_low_rg8sn;
        BufferAccessor2sn8 accessor_tangent_low_rg8sn;
        BufferAccessor1a2bgr10sn accessor_normal_a2bgr10sn;
        BufferAccessor1a2rgb10sn accessor_normal_a2rgb10sn;
        BufferAccessor1a2bgr10sn accessor_tangent_a2bgr10sn;
        BufferAccessor1a2rgb10sn accessor_tangent_a2rgb10sn;
        BufferAccessor2f accessor_texcoord;
        BufferAccessor2hf accessor_texcoord_hf;
        FormatAwareWriter format_writer;

    public:
        GeometryBuilder(GeometryCreater *pc);
        virtual ~GeometryBuilder();

        /**
         * 检查构建器是否有效
         * @return 如果顶点位置访问器有效返回true
         */
        bool IsValid() const { return accessor_position.IsValid(); }

        /**
         * 写入顶点位置
         * @param x, y, z 顶点坐标
         */
        inline void WriteVertex(float x, float y, float z)
        {
            if(accessor_position.IsValid())
                accessor_position->Write(x, y, z);
        }

        /**
         * 写入法线
         * @param x, y, z 法线坐标
         */
        void WriteNormal(float x, float y, float z);

        /**
         * 写入切线
         * @param x, y, z 切线坐标
         */
        void WriteTangent(float x, float y, float z);

        /**
         * 写入切线（含手性）
         * @param x, y, z 切线坐标
         * @param w 切线手性，通常为+1或-1
         */
        void WriteTangent(float x, float y, float z, float w);

        /**
         * 写入纹理坐标
         * @param u, v 纹理坐标
         */
        void WriteTexCoord(float u, float v);

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
         * 写入完整顶点数据（位置+法线+切线(含手性)+纹理坐标）
         */
        inline void WriteFullVertex(float px, float py, float pz,
                                   float nx, float ny, float nz,
                                   float tx, float ty, float tz, float tw,
                                   float u, float v)
        {
            WriteVertex(px, py, pz);
            WriteNormal(nx, ny, nz);
            WriteTangent(tx, ty, tz, tw);
            WriteTexCoord(u, v);
        }

        /**
         * 检查是否有法线缓冲
         */
        bool HasNormals()   const { return creater && creater->GetVAB(VAN::Normal)   != nullptr; }

        /**
         * 检查是否有切线缓冲
         */
        bool HasTangents()  const { return creater && creater->GetVAB(VAN::Tangent)  != nullptr; }

        /**
         * 检查是否有纹理坐标缓冲
         */
        bool HasTexCoords() const { return creater && creater->GetVAB(VAN::TexCoord) != nullptr; }
    };
}
