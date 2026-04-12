#pragma once

#include<cstdint>

namespace hgl::graph
{
    class GeometryCreater;

    namespace inline_geometry
    {
        enum class InlineGeoFormatPreset : uint8_t
        {
            Legacy = 0,
            NT_U8x2_U8x2_UV_HF16x2,
            NT_HF16x2_HF16x2_UV_HF16x2
        };

        /**
         * 统一格式写入入口（Commit 1: 默认 Legacy 透传）
         */
        class FormatAwareWriter
        {
        private:
            GeometryCreater *creater;
            InlineGeoFormatPreset preset;

        public:
            FormatAwareWriter(GeometryCreater *gc=nullptr,
                              InlineGeoFormatPreset p=InlineGeoFormatPreset::Legacy);

            bool IsValid() const { return creater != nullptr; }

            InlineGeoFormatPreset GetPreset() const { return preset; }
            void SetPreset(const InlineGeoFormatPreset p) { preset = p; }

            bool WritePosition(float x,float y,float z);
            bool WriteNormal(float x,float y,float z);
            bool WriteTangent(float x,float y,float z);
            bool WriteUV(float u,float v);

            bool WriteNormalTangentUV(float nx,float ny,float nz,
                                      float tx,float ty,float tz,
                                      float u,float v);
        };
    }//namespace inline_geometry
}//namespace hgl::graph
