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

        VkFormat position_format = VK_FORMAT_UNDEFINED;
        VkFormat normal_format = VK_FORMAT_UNDEFINED;
        VkFormat tangent_format = VK_FORMAT_UNDEFINED;
        VkFormat texcoord_format = VK_FORMAT_UNDEFINED;
        VkFormat color_format = VK_FORMAT_UNDEFINED;
        VkFormat luminance_format = VK_FORMAT_UNDEFINED;

        bool has_position = false;
        bool has_normals = false;
        bool has_tangents = false;
        bool has_texcoords = false;
        bool has_colors = false;
        bool has_luminance = false;

        BufferAccessor2f accessor_position2;
        BufferAccessor<VB2u8> accessor_position2u8;
        BufferAccessor3f accessor_position;
        BufferAccessor3f accessor_normal;
        BufferAccessor3f accessor_tangent;
        BufferAccessor4f accessor_tangent4;
        BufferAccessor2f accessor_texcoord;
        BufferAccessor4f accessor_color;
        BufferAccessor<VB1f> accessor_luminance;

        BufferAccessor<VB2hf>  accessor_normal_rg16f;
        BufferAccessor<VB2uf8> accessor_normal_rg8un;
        BufferAccessor<VB2sf8> accessor_normal_rg8sn;
        RawAccessorU32         accessor_normal_a2rgb10un;
        RawAccessorU32         accessor_normal_a2bgr10un;

        BufferAccessor<VB2hf>  accessor_tangent_rg16f;
        BufferAccessor<VB2uf8> accessor_tangent_rg8un;
        BufferAccessor<VB2sf8> accessor_tangent_rg8sn;
        RawAccessorU32         accessor_tangent_a2rgb10un;
        RawAccessorU32         accessor_tangent_a2bgr10un;

        BufferAccessor<VB2hf>   accessor_texcoord_rg16f;
        BufferAccessor<VB2uf16> accessor_texcoord_rg16un;
        BufferAccessor<VB2uf8>  accessor_texcoord_rg8un;

        BufferAccessor<VB4hf>   accessor_color_rgba16f;
        BufferAccessor<VB4uf16> accessor_color_rgba16un;
        BufferAccessor<VB4uf8>  accessor_color_rgba8un;
        RawAccessorU32          accessor_color_a2rgb10un;
        RawAccessorU32          accessor_color_a2bgr10un;

        BufferAccessor<VB1hf>   accessor_luminance_r16f;
        BufferAccessor<VB1uf16> accessor_luminance_r16un;
        BufferAccessor<VB1uf8>  accessor_luminance_r8un;
        BufferAccessor<VB1u32>  accessor_luminance_r32u;
        BufferAccessor<VB1u16>  accessor_luminance_r16u;
        BufferAccessor<VB1u8>   accessor_luminance_r8u;

    private:
        static float Clamp01(float v);
        static float ClampN1P1(float v);
        static int8 ToSnorm8(float v);
        static uint8 ToUnorm8(float v);
        static uint16 ToUnorm16(float v);
        static uint32 ToUnorm10(float v);

        static void EncodeOct2(float x, float y, float z, float &ox, float &oy);
        static uint32 PackA2R10G10B10_UNORM(uint32 r, uint32 g, uint32 b, uint32 a);
        static uint32 PackA2B10G10R10_UNORM(uint32 r, uint32 g, uint32 b, uint32 a);

        void WriteNormalByFormat(float x, float y, float z);
        void WriteTangentByFormat(float x, float y, float z);
        void WriteTangentByFormat(float x, float y, float z, float w);
        void WriteTexCoordByFormat(float u, float v);
        void WriteColorByFormat(float r, float g, float b, float a);
        void WriteLuminanceByFormat(float l);
        void WriteLuminanceByteByFormat(uint8 l);

    public:
        GeometryBuilder(GeometryCreater *pc);
        virtual ~GeometryBuilder();

        /**
         * 检查构建器是否有效
         * @return 如果顶点位置访问器有效返回true
         */
        bool IsValid() const { return has_position; }

        /**
         * 写入顶点位置
         * @param x, y, z 顶点坐标
         */
        inline void WriteVertex(float x, float y, float z)
        {
            if(accessor_position.IsValid())
                accessor_position->Write(x, y, z);
            else
            if(accessor_position2.IsValid())
                accessor_position2->Write(x, y);
            else
            if(accessor_position2u8.IsValid())
                accessor_position2u8->Write(uint8(x), uint8(y));
        }

        inline void WriteVertex(float x, float y)
        {
            if(accessor_position2.IsValid())
                accessor_position2->Write(x, y);
            else
            if(accessor_position2u8.IsValid())
                accessor_position2u8->Write(uint8(x), uint8(y));
            else
            if(accessor_position.IsValid())
                accessor_position->Write(x, y, 0.0f);
        }

        /**
         * 写入uint8位置
         * @param x, y uint8坐标
         */
        inline void WriteVertexU8(uint8 x, uint8 y)
        {
            if(accessor_position2u8.IsValid())
                accessor_position2u8->Write(x, y);
            else
            if(accessor_position2.IsValid())
                accessor_position2->Write(float(x), float(y));
            else
            if(accessor_position.IsValid())
                accessor_position->Write(float(x), float(y), 0.0f);
        }

        /**
         * 写入法线
         * @param x, y, z 法线坐标
         */
        inline void WriteNormal(float x, float y, float z)
        {
            WriteNormalByFormat(x, y, z);
        }

        /**
         * 写入切线
         * @param x, y, z 切线坐标
         */
        inline void WriteTangent(float x, float y, float z)
        {
            // default tangent.w = +1; TODO: compute handedness when tangent basis data is available
            WriteTangentByFormat(x, y, z, 1.0f);
        }

        inline void WriteTangent(float x, float y, float z, float w)
        {
            WriteTangentByFormat(x, y, z, w);
        }

        /**
         * 写入纹理坐标
         * @param u, v 纹理坐标
         */
        inline void WriteTexCoord(float u, float v)
        {
            WriteTexCoordByFormat(u, v);
        }

        /**
         * 写入颜色
         * @param r, g, b, a 颜色分量
         */
        inline void WriteColor(float r, float g, float b, float a)
        {
            WriteColorByFormat(r, g, b, a);
        }

        /**
         * 写入亮度
         * @param l 亮度值
         */
        inline void WriteLuminance(float l)
        {
            WriteLuminanceByFormat(l);
        }

        inline void WriteLuminance(uint8 l)
        {
            WriteLuminanceByteByFormat(l);
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
                                   float tw,
                                   float u, float v)
        {
            WriteVertex(px, py, pz);
            WriteNormal(nx, ny, nz);
            WriteTangent(tx, ty, tz, tw);
            WriteTexCoord(u, v);
        }

        inline void WriteFullVertex(float px, float py, float pz,
                                   float nx, float ny, float nz,
                                   float tx, float ty, float tz,
                                   float u, float v)
        {
            // default tangent.w = +1; TODO: compute handedness when tangent basis data is available
            WriteFullVertex(px, py, pz, nx, ny, nz, tx, ty, tz, 1.0f, u, v);
        }

        /**
         * 检查是否有法线缓冲
         */
        bool HasNormals() const { return has_normals; }

        /**
         * 检查是否有切线缓冲
         */
        bool HasTangents() const { return has_tangents; }

        /**
         * 检查是否有纹理坐标缓冲
         */
        bool HasTexCoords() const { return has_texcoords; }

        /**
         * 检查是否有颜色缓冲
         */
        bool HasColors() const { return has_colors; }

        /**
         * 检查是否有亮度缓冲
         */
        bool HasLuminance() const { return has_luminance; }
    };
}
