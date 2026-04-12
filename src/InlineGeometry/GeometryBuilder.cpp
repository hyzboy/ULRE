#include<hgl/graph/geo/GeometryBuilder.h>
#include<hgl/graph/geo/VKGeometry.h>
#include<hgl/math/HalfFloat.h>
#include<hgl/math/NormalData.h>
#include<algorithm>

namespace
{
    using namespace hgl;
    using namespace hgl::graph;
    using namespace hgl::graph::inline_geometry;

    static int32_t ClampSNorm10(const float value)
    {
        float clamped = value;

        if(clamped < -1.0f)
            clamped = -1.0f;
        else
        if(clamped > 1.0f)
            clamped = 1.0f;

        int32_t scaled = int32_t(clamped * 511.0f);

        if(scaled < -511)
            scaled = -511;
        else
        if(scaled > 511)
            scaled = 511;

        return scaled;
    }

    static int32_t ClampSNorm2(const float value)
    {
        return (value < 0.0f) ? -1 : 1;
    }

    static uint32_t PackA2B10G10R10SNorm(const float x,const float y,const float z,const float w)
    {
        const uint32_t r = uint32_t(ClampSNorm10(x)) & 0x3FFu;
        const uint32_t g = uint32_t(ClampSNorm10(y)) & 0x3FFu;
        const uint32_t b = uint32_t(ClampSNorm10(z)) & 0x3FFu;
        const uint32_t a = uint32_t(ClampSNorm2(w))  & 0x3u;

        return r | (g << 10u) | (b << 20u) | (a << 30u);
    }

    static uint32_t PackA2R10G10B10SNorm(const float x,const float y,const float z,const float w)
    {
        const uint32_t r = uint32_t(ClampSNorm10(x)) & 0x3FFu;
        const uint32_t g = uint32_t(ClampSNorm10(y)) & 0x3FFu;
        const uint32_t b = uint32_t(ClampSNorm10(z)) & 0x3FFu;
        const uint32_t a = uint32_t(ClampSNorm2(w))  & 0x3u;

        return b | (g << 10u) | (r << 20u) | (a << 30u);
    }

    static bool WriteVec2SNorm8EncodedNormal(BufferAccessor2sn8 &accessor,const float x,const float y,const float z)
    {
        if(!accessor.IsValid())
            return false;

        const hgl::Vector2f enc = hgl::Normal3to2(hgl::Vector3f(x,y,z));

        return accessor->Write(int8_t(std::clamp(enc.x * 2.0f - 1.0f, -1.0f, 1.0f) * 127.0f),
                               int8_t(std::clamp(enc.y * 2.0f - 1.0f, -1.0f, 1.0f) * 127.0f));
    }
}

namespace hgl::graph::inline_geometry
{
    GeometryBuilder::GeometryBuilder(GeometryCreater *pc)
        : creater(pc)
        , format_writer(pc,
                        pc ? pc->GetInlineGeoFormatPreset()
                           : InlineGeoFormatPreset::Legacy)
    {
        if(!pc)
            return;

        // 绑定 BufferAccessor 到 VAB
        VertexAttribBuffer *vab;
        const int32_t vertex_offset = pc->GetVertexOffset();
        const uint32_t vertex_count = pc->GetVertexCount();

        vab = pc->GetVAB(VAN::Position);
        if(vab)
            accessor_position.Bind(vab, vertex_offset, vertex_count);

        vab = pc->GetVAB(VAN::Normal);
        if(vab && vab->GetFormat() == PF_RGB32F)
            accessor_normal.Bind(vab, vertex_offset, vertex_count);
        else
        if(vab && vab->GetFormat() == PF_NORMAL_LOW)
            accessor_normal_low_rg8sn.Bind(vab, vertex_offset, vertex_count);
        else
        if(vab && vab->GetFormat() == HGL_NT_PACK_FMT_A2BGR10_SNORM)
            accessor_normal_a2bgr10sn.Bind(vab, vertex_offset, vertex_count);
        else
        if(vab && vab->GetFormat() == HGL_NT_PACK_FMT_A2RGB10_SNORM)
            accessor_normal_a2rgb10sn.Bind(vab, vertex_offset, vertex_count);

        vab = pc->GetVAB(VAN::Tangent);
        if(vab && vab->GetFormat() == PF_RGB32F)
            accessor_tangent.Bind(vab, vertex_offset, vertex_count);
        else
        if(vab && vab->GetFormat() == PF_TANGENT_LOW)
            accessor_tangent_low_rg8sn.Bind(vab, vertex_offset, vertex_count);
        else
        if(vab && vab->GetFormat() == HGL_NT_PACK_FMT_A2BGR10_SNORM)
            accessor_tangent_a2bgr10sn.Bind(vab, vertex_offset, vertex_count);
        else
        if(vab && vab->GetFormat() == HGL_NT_PACK_FMT_A2RGB10_SNORM)
            accessor_tangent_a2rgb10sn.Bind(vab, vertex_offset, vertex_count);

        vab = pc->GetVAB(VAN::TexCoord);
        if(vab && vab->GetFormat() == PF_RG32F)
            accessor_texcoord.Bind(vab, vertex_offset, vertex_count);
        else if(vab && vab->GetFormat() == PF_RG16F)
            accessor_texcoord_hf.Bind(vab, vertex_offset, vertex_count);
    }

    GeometryBuilder::~GeometryBuilder()
    {
        // BufferAccessor 自动管理生命周期，无需手动清理
    }

    void GeometryBuilder::WriteNormal(float x, float y, float z)
    {
        if(accessor_normal.IsValid())
        {
            accessor_normal->Write(x, y, z);
            return;
        }

        if(WriteVec2SNorm8EncodedNormal(accessor_normal_low_rg8sn,x,y,z))
            return;

        if(accessor_normal_a2bgr10sn.IsValid())
        {
            accessor_normal_a2bgr10sn->Write(PackA2B10G10R10SNorm(x, y, z, 1.0f));
            return;
        }

        if(accessor_normal_a2rgb10sn.IsValid())
        {
            accessor_normal_a2rgb10sn->Write(PackA2R10G10B10SNorm(x, y, z, 1.0f));
            return;
        }

        if(format_writer.IsValid())
            format_writer.WriteNormal(x, y, z);
    }

    void GeometryBuilder::WriteTangent(float x, float y, float z)
    {
        WriteTangent(x, y, z, 1.0f);
    }

    void GeometryBuilder::WriteTangent(float x, float y, float z, float w)
    {
        if(accessor_tangent.IsValid())
        {
            accessor_tangent->Write(x, y, z);
            return;
        }

        if(WriteVec2SNorm8EncodedNormal(accessor_tangent_low_rg8sn,x,y,z))
            return;

        if(accessor_tangent_a2bgr10sn.IsValid())
        {
            accessor_tangent_a2bgr10sn->Write(PackA2B10G10R10SNorm(x, y, z, w));
            return;
        }

        if(accessor_tangent_a2rgb10sn.IsValid())
        {
            accessor_tangent_a2rgb10sn->Write(PackA2R10G10B10SNorm(x, y, z, w));
            return;
        }

        if(format_writer.IsValid())
            format_writer.WriteTangent(x, y, z, w);
    }

    void GeometryBuilder::WriteTexCoord(float u, float v)
    {
        if(accessor_texcoord.IsValid())
        {
            accessor_texcoord->Write(u, v);
            return;
        }

        if(accessor_texcoord_hf.IsValid())
        {
            const float src[2] = {u, v};
            half_float dst[2] = {};
            math::Float32toFloat16(dst, src, 2);
            accessor_texcoord_hf->Write(dst[0], dst[1]);
            return;
        }

        if(format_writer.IsValid())
            format_writer.WriteUV(u, v);
    }
}
