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

    static bool WriteVec2SNorm8EncodedNormal(BufferAccessor2sn8 &accessor,const float x,const float y,const float z)
    {
        if(!accessor.IsValid())
            return false;

        const uint16 enc = hgl::Normal3to2u8(hgl::Vector3f(x,y,z));
        return accessor->Write(int8(enc & 0xFF),
                               int8((enc >> 8) & 0xFF));
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
            accessor_normal_a2bgr10sn->Write(int32(hgl::Normal3to4u10a2BGR(hgl::Vector3f(x, y, z))));
            return;
        }

        if(accessor_normal_a2rgb10sn.IsValid())
        {
            accessor_normal_a2rgb10sn->Write(int32(hgl::Normal3to4u10a2RGB(hgl::Vector3f(x, y, z))));
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
            accessor_tangent_a2bgr10sn->Write(int32(hgl::Normal3Tangent1to4u10a2BGR(hgl::Vector3f(x, y, z), w)));
            return;
        }

        if(accessor_tangent_a2rgb10sn.IsValid())
        {
            accessor_tangent_a2rgb10sn->Write(int32(hgl::Normal3Tangent1to4u10a2RGB(hgl::Vector3f(x, y, z), w)));
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
