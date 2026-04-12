#include<hgl/graph/geo/FormatAwareWriter.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/math/HalfFloat.h>
#include<hgl/math/NormalData.h>

namespace
{
    using namespace hgl;
    using namespace hgl::graph;
    using namespace hgl::graph::inline_geometry;

    static int8 ClampSNorm8(const float value)
    {
        float clamped = value;

        if(clamped < -1.0f)
            clamped = -1.0f;
        else
        if(clamped > 1.0f)
            clamped = 1.0f;

        int scaled = int(clamped * 127.0f);

        if(scaled < -127)
            scaled = -127;
        else
        if(scaled > 127)
            scaled = 127;

        return int8(scaled);
    }

    static int32 ClampSNorm10(const float value)
    {
        float clamped = value;

        if(clamped < -1.0f)
            clamped = -1.0f;
        else
        if(clamped > 1.0f)
            clamped = 1.0f;

        int32 scaled = int32(clamped * 511.0f);

        if(scaled < -511)
            scaled = -511;
        else
        if(scaled > 511)
            scaled = 511;

        return scaled;
    }

    static int32 ClampSNorm2(const float value)
    {
        return (value < 0.0f) ? -1 : 1;
    }

    static uint32 PackA2B10G10R10SNorm(const float x,const float y,const float z,const float w)
    {
        const uint32 r = uint32(ClampSNorm10(x)) & 0x3FFu;
        const uint32 g = uint32(ClampSNorm10(y)) & 0x3FFu;
        const uint32 b = uint32(ClampSNorm10(z)) & 0x3FFu;
        const uint32 a = uint32(ClampSNorm2(w))  & 0x3u;

        return r | (g << 10u) | (b << 20u) | (a << 30u);
    }

    static uint32 PackA2R10G10B10SNorm(const float x,const float y,const float z,const float w)
    {
        const uint32 r = uint32(ClampSNorm10(x)) & 0x3FFu;
        const uint32 g = uint32(ClampSNorm10(y)) & 0x3FFu;
        const uint32 b = uint32(ClampSNorm10(z)) & 0x3FFu;
        const uint32 a = uint32(ClampSNorm2(w))  & 0x3u;

        return b | (g << 10u) | (r << 20u) | (a << 30u);
    }

    static bool WriteHalf2(BufferAccessor2hf &accessor,const float x,const float y)
    {
        if(!accessor.IsValid())
            return false;

        const float input[2]={x,y};
        half_float output[2]={};

        math::Float32toFloat16(output,input,2);

        return accessor->Write(output[0],output[1]);
    }

    static bool WriteHalf4(BufferAccessor4hf &accessor,const float x,const float y,const float z,const float w)
    {
        if(!accessor.IsValid())
            return false;

        const float input[4]={x,y,z,w};
        half_float output[4]={};

        math::Float32toFloat16(output,input,4);

        return accessor->Write(output[0],output[1],output[2],output[3]);
    }

    static bool WriteVec4SNorm8(BufferAccessor4sn8 &accessor,const float x,const float y,const float z,const float w)
    {
        if(!accessor.IsValid())
            return false;

        return accessor->Write(ClampSNorm8(x),
                               ClampSNorm8(y),
                               ClampSNorm8(z),
                               ClampSNorm8(w));
    }

    static bool WriteVec2SNorm8EncodedNormal(BufferAccessor2sn8 &accessor,const float x,const float y,const float z)
    {
        if(!accessor.IsValid())
            return false;

        const Vector2f enc = Normal3to2(Vector3f(x,y,z));

        // PF_RG8SN storage expects signed normalized bytes.
        return accessor->Write(ClampSNorm8(enc.x * 2.0f - 1.0f),
                               ClampSNorm8(enc.y * 2.0f - 1.0f));
    }

    static bool WriteVec4HF16(BufferAccessor4hf &accessor,const float x,const float y,const float z,const float w)
    {
        if(!accessor.IsValid())
            return false;

        return WriteHalf4(accessor,x,y,z,w);
    }

    static bool WriteVec4A2BGR10SN(BufferAccessor1a2bgr10sn &accessor,const float x,const float y,const float z,const float w)
    {
        if(!accessor.IsValid())
            return false;

        return accessor->Write(PackA2B10G10R10SNorm(x,y,z,w));
    }

    static bool WriteVec4A2RGB10SN(BufferAccessor1a2rgb10sn &accessor,const float x,const float y,const float z,const float w)
    {
        if(!accessor.IsValid())
            return false;

        return accessor->Write(PackA2R10G10B10SNorm(x,y,z,w));
    }
}

namespace hgl::graph::inline_geometry
{
    FormatAwareWriter::FormatAwareWriter(GeometryCreater *gc,
                                         const InlineGeoFormatPreset p)
        : creater(gc), preset(p)
    {
        if(!gc)
            return;

        const int32_t vertex_offset = gc->GetVertexOffset();
        const uint32_t vertex_count = gc->GetVertexCount();

        if(VAB *vab = gc->GetVAB(VAN::Position))
        {
            if(vab->GetFormat() == PF_RGB32F)
                accessor_position_f32.Bind(vab,vertex_offset,vertex_count);
        }

        if(VAB *vab = gc->GetVAB(VAN::Normal))
        {
            switch(vab->GetFormat())
            {
                case PF_RGB32F:     accessor_normal_f32.Bind(vab,vertex_offset,vertex_count); break;
                case PF_NORMAL_LOW: accessor_normal_low_rg8sn.Bind(vab,vertex_offset,vertex_count); break;
                case PF_RGBA8SN:    accessor_normal_sn8x4.Bind(vab,vertex_offset,vertex_count); break;
                case PF_RGBA16F:    accessor_normal_hf16x4.Bind(vab,vertex_offset,vertex_count); break;
                case HGL_NT_PACK_FMT_A2BGR10_SNORM:  accessor_normal_a2bgr10sn.Bind(vab,vertex_offset,vertex_count); break;
                case HGL_NT_PACK_FMT_A2RGB10_SNORM:  accessor_normal_a2rgb10sn.Bind(vab,vertex_offset,vertex_count); break;
                default: break;
            }
        }

        if(VAB *vab = gc->GetVAB(VAN::Tangent))
        {
            switch(vab->GetFormat())
            {
                case PF_RGB32F:     accessor_tangent_f32.Bind(vab,vertex_offset,vertex_count); break;
                case PF_TANGENT_LOW: accessor_tangent_low_rg8sn.Bind(vab,vertex_offset,vertex_count); break;
                case PF_RGBA8SN:    accessor_tangent_sn8x4.Bind(vab,vertex_offset,vertex_count); break;
                case PF_RGBA16F:    accessor_tangent_hf16x4.Bind(vab,vertex_offset,vertex_count); break;
                case HGL_NT_PACK_FMT_A2BGR10_SNORM:  accessor_tangent_a2bgr10sn.Bind(vab,vertex_offset,vertex_count); break;
                case HGL_NT_PACK_FMT_A2RGB10_SNORM:  accessor_tangent_a2rgb10sn.Bind(vab,vertex_offset,vertex_count); break;
                default: break;
            }
        }

        if(VAB *vab = gc->GetVAB(VAN::TexCoord))
        {
            switch(vab->GetFormat())
            {
                case PF_RG32F: accessor_uv_f32.Bind(vab,vertex_offset,vertex_count); break;
                case PF_RG16F: accessor_uv_hf16x2.Bind(vab,vertex_offset,vertex_count); break;
                default: break;
            }
        }
    }

    bool FormatAwareWriter::WritePosition(float x,float y,float z)
    {
        if(!accessor_position_f32.IsValid())
            return false;

        accessor_position_f32->Write(x,y,z);
        return true;
    }

    bool FormatAwareWriter::WriteNormal(float x,float y,float z)
    {
        if(WriteVec2SNorm8EncodedNormal(accessor_normal_low_rg8sn,x,y,z))
            return true;

        if(WriteVec4A2BGR10SN(accessor_normal_a2bgr10sn,x,y,z,1.0f))
            return true;

        if(WriteVec4A2RGB10SN(accessor_normal_a2rgb10sn,x,y,z,1.0f))
            return true;

        switch(preset)
        {
            case InlineGeoFormatPreset::NT_SN8x4_SN8x4_UV_HF16x2:
                if(WriteVec4SNorm8(accessor_normal_sn8x4,x,y,z,1.0f))
                    return true;
                break;

            case InlineGeoFormatPreset::NT_HF16x4_HF16x4_UV_HF16x2:
                if(WriteVec4HF16(accessor_normal_hf16x4,x,y,z,1.0f))
                    return true;
                break;

            case InlineGeoFormatPreset::NT_A2BGR10SN_A2BGR10SN_UV_HF16x2:
                if(WriteVec4A2BGR10SN(accessor_normal_a2bgr10sn,x,y,z,1.0f))
                    return true;
                break;

            case InlineGeoFormatPreset::Legacy:
            default:
                break;
        }

        if(!accessor_normal_f32.IsValid())
            return false;

        accessor_normal_f32->Write(x,y,z);
        return true;
    }

    bool FormatAwareWriter::WriteTangent(float x,float y,float z)
    {
        return WriteTangent(x,y,z,1.0f);
    }

    bool FormatAwareWriter::WriteTangent(float x,float y,float z,float w)
    {
        if(WriteVec2SNorm8EncodedNormal(accessor_tangent_low_rg8sn,x,y,z))
            return true;

        if(WriteVec4A2BGR10SN(accessor_tangent_a2bgr10sn,x,y,z,w))
            return true;

        if(WriteVec4A2RGB10SN(accessor_tangent_a2rgb10sn,x,y,z,w))
            return true;

        switch(preset)
        {
            case InlineGeoFormatPreset::NT_SN8x4_SN8x4_UV_HF16x2:
                if(WriteVec4SNorm8(accessor_tangent_sn8x4,x,y,z,w))
                    return true;
                break;

            case InlineGeoFormatPreset::NT_HF16x4_HF16x4_UV_HF16x2:
                if(WriteVec4HF16(accessor_tangent_hf16x4,x,y,z,w))
                    return true;
                break;

            case InlineGeoFormatPreset::NT_A2BGR10SN_A2BGR10SN_UV_HF16x2:
                if(WriteVec4A2BGR10SN(accessor_tangent_a2bgr10sn,x,y,z,w))
                    return true;
                break;

            case InlineGeoFormatPreset::Legacy:
            default:
                break;
        }

        if(!accessor_tangent_f32.IsValid())
            return false;

        accessor_tangent_f32->Write(x,y,z);
        return true;
    }

    bool FormatAwareWriter::WriteUV(float u,float v)
    {
        if(WriteHalf2(accessor_uv_hf16x2,u,v))
            return true;

        switch(preset)
        {
            case InlineGeoFormatPreset::NT_SN8x4_SN8x4_UV_HF16x2:
            case InlineGeoFormatPreset::NT_HF16x4_HF16x4_UV_HF16x2:
            case InlineGeoFormatPreset::NT_A2BGR10SN_A2BGR10SN_UV_HF16x2:
            {
                // Half path already tried above.
                break;
            }

            case InlineGeoFormatPreset::Legacy:
            default:
                break;
        }

        if(!accessor_uv_f32.IsValid())
            return false;

        accessor_uv_f32->Write(u,v);
        return true;
    }

    bool FormatAwareWriter::WriteNormalTangentUV(float nx,float ny,float nz,
                                                 float tx,float ty,float tz,
                                                 float u,float v)
    {
        const bool n_ok = WriteNormal(nx,ny,nz);
        const bool t_ok = WriteTangent(tx,ty,tz);
        const bool u_ok = WriteUV(u,v);

        return n_ok && t_ok && u_ok;
    }

    bool FormatAwareWriter::WriteNormalTangentUV(float nx,float ny,float nz,
                                                 float tx,float ty,float tz,float tw,
                                                 float u,float v)
    {
        const bool n_ok = WriteNormal(nx,ny,nz);
        const bool t_ok = WriteTangent(tx,ty,tz,tw);
        const bool u_ok = WriteUV(u,v);

        return n_ok && t_ok && u_ok;
    }
}//namespace hgl::graph::inline_geometry
