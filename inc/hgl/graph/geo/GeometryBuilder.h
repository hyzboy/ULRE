#pragma once

#include<hgl/graph/geo/GeometryCreater.h>
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
        BufferAccessor2f accessor_texcoord;

        // 压缩格式法线访问器（发行版：Normal 存 RG16F——xy 半浮点，z 由 shader 重建）
        // 由构造函数按 VAB format 分派（VK_FORMAT_R16G16_SFLOAT 时使用）
        BufferAccessor2hf accessor_normal_2hf;

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
        inline void WriteNormal(float x, float y, float z)
        {
            if(accessor_normal_2hf.IsValid())
            {
                // RG16F 压缩：只存 xy（半浮点）——z 由 shader 重建并 normalize
                accessor_normal_2hf->Write(x, y);
            }
            else if(accessor_normal.IsValid())
            {
                accessor_normal->Write(x, y, z);
            }
        }

        /**
         * 写入切线
         * @param x, y, z 切线坐标
         */
        inline void WriteTangent(float x, float y, float z)
        {
            if(accessor_tangent.IsValid())
                accessor_tangent->Write(x, y, z);
        }

        /**
         * 写入纹理坐标
         * @param u, v 纹理坐标
         */
        inline void WriteTexCoord(float u, float v)
        {
            if(accessor_texcoord.IsValid())
                accessor_texcoord->Write(u, v);
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
        bool HasNormals() const { return accessor_normal.IsValid(); }

        /**
         * 检查是否有切线缓冲
         */
        bool HasTangents() const { return accessor_tangent.IsValid(); }

        /**
         * 检查是否有纹理坐标缓冲
         */
        bool HasTexCoords() const { return accessor_texcoord.IsValid(); }
    };
}
