#pragma once

#include<cstdint>
#include<hgl/vk/VKBufferAccessor.h>

namespace hgl::graph
{
    class GeometryCreater;

    namespace inline_geometry
    {
        enum class InlineGeoFormatPreset : uint8_t
        {
            Legacy = 0,
            NT_SN8x4_SN8x4_UV_HF16x2,
            NT_HF16x4_HF16x4_UV_HF16x2,
            NT_A2BGR10SN_A2BGR10SN_UV_HF16x2
        };

        /**
         * 统一格式写入入口（Commit 1: 默认 Legacy 透传）
         */
        class FormatAwareWriter
        {
        private:
            GeometryCreater *creater;
            InlineGeoFormatPreset preset;

            BufferAccessor3f accessor_position_f32;

            BufferAccessor3f accessor_normal_f32;
            BufferAccessor4sn8 accessor_normal_sn8x4;
            BufferAccessor4hf accessor_normal_hf16x4;
            BufferAccessor1a2bgr10sn accessor_normal_a2bgr10sn;
            BufferAccessor1a2rgb10sn accessor_normal_a2rgb10sn;

            BufferAccessor3f accessor_tangent_f32;
            BufferAccessor4sn8 accessor_tangent_sn8x4;
            BufferAccessor4hf accessor_tangent_hf16x4;
            BufferAccessor1a2bgr10sn accessor_tangent_a2bgr10sn;
            BufferAccessor1a2rgb10sn accessor_tangent_a2rgb10sn;

            BufferAccessor2f accessor_uv_f32;
            BufferAccessor2hf accessor_uv_hf16x2;

        public:
            FormatAwareWriter(GeometryCreater *gc=nullptr,
                              InlineGeoFormatPreset p=InlineGeoFormatPreset::Legacy);

            bool IsValid() const { return creater != nullptr; }

            InlineGeoFormatPreset GetPreset() const { return preset; }
            void SetPreset(const InlineGeoFormatPreset p) { preset = p; }

            bool WritePosition(float x,float y,float z);
            bool WriteNormal(float x,float y,float z);
            bool WriteTangent(float x,float y,float z);
            bool WriteTangent(float x,float y,float z,float w);
            bool WriteUV(float u,float v);

            bool WriteNormalTangentUV(float nx,float ny,float nz,
                                      float tx,float ty,float tz,
                                      float u,float v);
            bool WriteNormalTangentUV(float nx,float ny,float nz,
                                      float tx,float ty,float tz,float tw,
                                      float u,float v);
        };
    }//namespace inline_geometry
}//namespace hgl::graph
