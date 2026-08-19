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
        BufferAccessor4f accessor_tangent_4f;   // 切线 V4F（含 w 分量——唯一切线访问器）
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
         * 写入法线（只有 Normal）——输入 float×3，写入格式由内部按 VAB 自动选择
         * （V3F → 3f 直写；RG16F → octahedral 编码 + half 位模式）
         */
        inline void WriteNTB(float nx, float ny, float nz)
        {
            if(accessor_normal_2hf.IsValid())
            {
                // RG16F 压缩：octahedral 编码（2 分量完整方向——z 符号保留）
                // 注意：VB2hf 的 T 是 uint16（half 位模式）——必须显式 FloatToHalf
                float p, q;
                EncodeOctahedralNormal(nx, ny, nz, p, q);
                accessor_normal_2hf->Write(FloatToHalf(p), FloatToHalf(q));
            }
            else if(accessor_normal.IsValid())
            {
                accessor_normal->Write(nx, ny, nz);
            }
        }

        /** 写入法线（Vector3f 版——只有 Normal） */
        inline void WriteNTB(const Vector3f &normal)
        {
            WriteNTB(normal.x, normal.y, normal.z);
        }

        /**
         * 写入完整 NTB（法线+切线+副法线）——输入 float×3，写入格式与外部调用无关
         * 注：binormal 当前无独立 VAB（GeometryVertexFormat 无 Binormal 语义）——暂不写入
         */
        /**
         * 写入完整 NTB（法线+切线+副法线）——输入 float×3/×4，写入格式与外部调用无关
         * 切线为 vec4（w 分量 = 面朝向符号——normalmap 副法线方向）
         * 注：binormal 当前无独立 VAB（format 无 Binormal 语义）——暂不写入
         */
        inline void WriteNTB(const Vector3f &normal, const Vector4f &tangent, const Vector3f &binormal)
        {
            WriteNTB(normal.x, normal.y, normal.z);

            if(accessor_tangent_4f.IsValid())
                accessor_tangent_4f->Write(tangent.x, tangent.y, tangent.z, tangent.w);
            // binormal：GeometryVertexFormat 无 Binormal 语义——暂不写入（未来扩展）
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
            WriteNTB(nx, ny, nz);
            // 切线写入（V4F 唯一格式——w 固定 1；调用方无面朝向信息时默认右手系）
            if(accessor_tangent_4f.IsValid())
                accessor_tangent_4f->Write(tx, ty, tz, 1.0f);
            WriteTexCoord(u, v);
        }

        /**
         * 检查是否有法线缓冲
         */
        bool HasNormals() const { return accessor_normal.IsValid(); }

        /**
         * 检查是否有切线缓冲
         */
        bool HasTangents() const { return accessor_tangent_4f.IsValid(); }

        /**
         * 检查是否有纹理坐标缓冲
         */
        bool HasTexCoords() const { return accessor_texcoord.IsValid(); }
    };
}
