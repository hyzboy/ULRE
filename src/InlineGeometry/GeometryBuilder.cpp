#include<hgl/graph/geo/GeometryBuilder.h>
#include<hgl/graph/geo/VKGeometry.h>
#include<cmath>

namespace hgl::graph::inline_geometry
{
    static inline float SignNotZero(const float v)
    {
        return v >= 0.0f ? 1.0f : -1.0f;
    }

    float GeometryBuilder::Clamp01(float v)
    {
        if(v < 0.0f) return 0.0f;
        if(v > 1.0f) return 1.0f;
        return v;
    }

    float GeometryBuilder::ClampN1P1(float v)
    {
        if(v < -1.0f) return -1.0f;
        if(v > 1.0f) return 1.0f;
        return v;
    }

    int8 GeometryBuilder::ToSnorm8(float v)
    {
        const float c = ClampN1P1(v);
        const int iv = int(std::round(c * 127.0f));
        if(iv < -128) return int8(-128);
        if(iv > 127) return int8(127);
        return int8(iv);
    }

    uint8 GeometryBuilder::ToUnorm8(float v)
    {
        const float c = Clamp01(v);
        const int iv = int(std::round(c * 255.0f));
        if(iv < 0) return uint8(0);
        if(iv > 255) return uint8(255);
        return uint8(iv);
    }

    uint16 GeometryBuilder::ToUnorm16(float v)
    {
        const float c = Clamp01(v);
        const int iv = int(std::round(c * 65535.0f));
        if(iv < 0) return uint16(0);
        if(iv > 65535) return uint16(65535);
        return uint16(iv);
    }

    uint32 GeometryBuilder::ToUnorm10(float v)
    {
        const float c = Clamp01(v);
        const int iv = int(std::round(c * 1023.0f));
        if(iv < 0) return uint32(0);
        if(iv > 1023) return uint32(1023);
        return uint32(iv);
    }

    void GeometryBuilder::EncodeOct2(float x, float y, float z, float &ox, float &oy)
    {
        const float ax = std::fabs(x);
        const float ay = std::fabs(y);
        const float az = std::fabs(z);
        const float inv_l1 = 1.0f / (ax + ay + az + 1e-20f);

        float nx = x * inv_l1;
        float ny = y * inv_l1;
        const float nz = z * inv_l1;

        if(nz < 0.0f)
        {
            const float old_x = nx;
            nx = (1.0f - std::fabs(ny)) * SignNotZero(old_x);
            ny = (1.0f - std::fabs(old_x)) * SignNotZero(ny);
        }

        ox = ClampN1P1(nx);
        oy = ClampN1P1(ny);
    }

    uint32 GeometryBuilder::PackA2R10G10B10_UNORM(uint32 r, uint32 g, uint32 b, uint32 a)
    {
        return ((a & 0x3u) << 30)
             | ((r & 0x3ffu) << 20)
             | ((g & 0x3ffu) << 10)
             |  (b & 0x3ffu);
    }

    uint32 GeometryBuilder::PackA2B10G10R10_UNORM(uint32 r, uint32 g, uint32 b, uint32 a)
    {
        return ((a & 0x3u) << 30)
             | ((b & 0x3ffu) << 20)
             | ((g & 0x3ffu) << 10)
             |  (r & 0x3ffu);
    }

    void GeometryBuilder::WriteNormalByFormat(float x, float y, float z)
    {
        if(!has_normals)
            return;

        switch(normal_format)
        {
            case PF_RGB32F:
                if(accessor_normal.IsValid()) accessor_normal->Write(x, y, z);
            break;

            case PF_RG16F:
                if(accessor_normal_rg16f.IsValid())
                {
                    float ox, oy;
                    EncodeOct2(x, y, z, ox, oy);
                    accessor_normal_rg16f->Write(ox, oy);
                }
            break;

            case PF_RG8UN:
                if(accessor_normal_rg8un.IsValid())
                {
                    float ox, oy;
                    EncodeOct2(x, y, z, ox, oy);
                    accessor_normal_rg8un->Write(ToUnorm8(ox * 0.5f + 0.5f),
                                                 ToUnorm8(oy * 0.5f + 0.5f));
                }
            break;

            case PF_RG8SN:
                if(accessor_normal_rg8sn.IsValid())
                {
                    float ox, oy;
                    EncodeOct2(x, y, z, ox, oy);
                    accessor_normal_rg8sn->Write(ToSnorm8(ox), ToSnorm8(oy));
                }
            break;

            case PF_A2RGB10UN:
                if(accessor_normal_a2rgb10un.IsValid())
                {
                    float ox, oy;
                    EncodeOct2(x, y, z, ox, oy);
                    const uint32 r = ToUnorm10(ox * 0.5f + 0.5f);
                    const uint32 g = ToUnorm10(oy * 0.5f + 0.5f);
                    const uint32 b = ToUnorm10(0.5f);
                    accessor_normal_a2rgb10un->Write(PackA2R10G10B10_UNORM(r, g, b, 3u));
                }
            break;

            case PF_A2BGR10UN:
                if(accessor_normal_a2bgr10un.IsValid())
                {
                    float ox, oy;
                    EncodeOct2(x, y, z, ox, oy);
                    const uint32 r = ToUnorm10(ox * 0.5f + 0.5f);
                    const uint32 g = ToUnorm10(oy * 0.5f + 0.5f);
                    const uint32 b = ToUnorm10(0.5f);
                    accessor_normal_a2bgr10un->Write(PackA2B10G10R10_UNORM(r, g, b, 3u));
                }
            break;

            default:
            break;
        }
    }

    void GeometryBuilder::WriteTangentByFormat(float x, float y, float z)
    {
        if(!has_tangents)
            return;

        switch(tangent_format)
        {
            case PF_RGB32F:
                if(accessor_tangent.IsValid()) accessor_tangent->Write(x, y, z);
            break;

            case PF_RG16F:
                if(accessor_tangent_rg16f.IsValid())
                {
                    float ox, oy;
                    EncodeOct2(x, y, z, ox, oy);
                    accessor_tangent_rg16f->Write(ox, oy);
                }
            break;

            case PF_RG8UN:
                if(accessor_tangent_rg8un.IsValid())
                {
                    float ox, oy;
                    EncodeOct2(x, y, z, ox, oy);
                    accessor_tangent_rg8un->Write(ToUnorm8(ox * 0.5f + 0.5f),
                                                  ToUnorm8(oy * 0.5f + 0.5f));
                }
            break;

            case PF_RG8SN:
                if(accessor_tangent_rg8sn.IsValid())
                {
                    float ox, oy;
                    EncodeOct2(x, y, z, ox, oy);
                    accessor_tangent_rg8sn->Write(ToSnorm8(ox), ToSnorm8(oy));
                }
            break;

            case PF_A2RGB10UN:
                if(accessor_tangent_a2rgb10un.IsValid())
                {
                    float ox, oy;
                    EncodeOct2(x, y, z, ox, oy);
                    const uint32 r = ToUnorm10(ox * 0.5f + 0.5f);
                    const uint32 g = ToUnorm10(oy * 0.5f + 0.5f);
                    const uint32 b = ToUnorm10(0.5f);
                    accessor_tangent_a2rgb10un->Write(PackA2R10G10B10_UNORM(r, g, b, 3u));
                }
            break;

            case PF_A2BGR10UN:
                if(accessor_tangent_a2bgr10un.IsValid())
                {
                    float ox, oy;
                    EncodeOct2(x, y, z, ox, oy);
                    const uint32 r = ToUnorm10(ox * 0.5f + 0.5f);
                    const uint32 g = ToUnorm10(oy * 0.5f + 0.5f);
                    const uint32 b = ToUnorm10(0.5f);
                    accessor_tangent_a2bgr10un->Write(PackA2B10G10R10_UNORM(r, g, b, 3u));
                }
            break;

            default:
            break;
        }
    }

    void GeometryBuilder::WriteTexCoordByFormat(float u, float v)
    {
        if(!has_texcoords)
            return;

        switch(texcoord_format)
        {
            case PF_RG32F:
                if(accessor_texcoord.IsValid()) accessor_texcoord->Write(u, v);
            break;

            case PF_RG16F:
                if(accessor_texcoord_rg16f.IsValid()) accessor_texcoord_rg16f->Write(u, v);
            break;

            case PF_RG16UN:
                if(accessor_texcoord_rg16un.IsValid())
                    accessor_texcoord_rg16un->Write(ToUnorm16(u), ToUnorm16(v));
            break;

            case PF_RG8UN:
                if(accessor_texcoord_rg8un.IsValid())
                    accessor_texcoord_rg8un->Write(ToUnorm8(u), ToUnorm8(v));
            break;

            default:
            break;
        }
    }

    void GeometryBuilder::WriteColorByFormat(float r, float g, float b, float a)
    {
        if(!has_colors)
            return;

        switch(color_format)
        {
            case PF_RGBA32F:
                if(accessor_color.IsValid()) accessor_color->Write(r, g, b, a);
            break;

            case PF_RGBA16F:
                if(accessor_color_rgba16f.IsValid()) accessor_color_rgba16f->Write(r, g, b, a);
            break;

            case PF_RGBA16UN:
                if(accessor_color_rgba16un.IsValid())
                    accessor_color_rgba16un->Write(ToUnorm16(r), ToUnorm16(g), ToUnorm16(b), ToUnorm16(a));
            break;

            case PF_RGBA8UN:
                if(accessor_color_rgba8un.IsValid())
                    accessor_color_rgba8un->Write(ToUnorm8(r), ToUnorm8(g), ToUnorm8(b), ToUnorm8(a));
            break;

            case PF_A2RGB10UN:
                if(accessor_color_a2rgb10un.IsValid())
                {
                    const uint32 pr = ToUnorm10(r);
                    const uint32 pg = ToUnorm10(g);
                    const uint32 pb = ToUnorm10(b);
                    const uint32 pa = (Clamp01(a) >= 0.5f) ? 3u : 0u;
                    accessor_color_a2rgb10un->Write(PackA2R10G10B10_UNORM(pr, pg, pb, pa));
                }
            break;

            case PF_A2BGR10UN:
                if(accessor_color_a2bgr10un.IsValid())
                {
                    const uint32 pr = ToUnorm10(r);
                    const uint32 pg = ToUnorm10(g);
                    const uint32 pb = ToUnorm10(b);
                    const uint32 pa = (Clamp01(a) >= 0.5f) ? 3u : 0u;
                    accessor_color_a2bgr10un->Write(PackA2B10G10R10_UNORM(pr, pg, pb, pa));
                }
            break;

            default:
            break;
        }
    }

    void GeometryBuilder::WriteLuminanceByFormat(float l)
    {
        if(!has_luminance)
            return;

        switch(luminance_format)
        {
            case PF_R32F:
                if(accessor_luminance.IsValid()) accessor_luminance->Write(l);
            break;

            case PF_R16F:
                if(accessor_luminance_r16f.IsValid()) accessor_luminance_r16f->Write(l);
            break;

            case PF_R16UN:
                if(accessor_luminance_r16un.IsValid()) accessor_luminance_r16un->Write(ToUnorm16(l));
            break;

            case PF_R8UN:
                if(accessor_luminance_r8un.IsValid()) accessor_luminance_r8un->Write(ToUnorm8(l));
            break;

            default:
            break;
        }
    }

    GeometryBuilder::GeometryBuilder(GeometryCreater *pc)
        : creater(pc)
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
        if(vab)
        {
            normal_format = vab->GetFormat();

            switch(normal_format)
            {
                case PF_RGB32F: accessor_normal.Bind(vab, vertex_offset, vertex_count); break;
                case PF_RG16F: accessor_normal_rg16f.Bind(vab, vertex_offset, vertex_count); break;
                case PF_RG8UN: accessor_normal_rg8un.Bind(vab, vertex_offset, vertex_count); break;
                case PF_RG8SN: accessor_normal_rg8sn.Bind(vab, vertex_offset, vertex_count); break;
                case PF_A2RGB10UN: accessor_normal_a2rgb10un.Bind(vab, vertex_offset, vertex_count); break;
                case PF_A2BGR10UN: accessor_normal_a2bgr10un.Bind(vab, vertex_offset, vertex_count); break;
                default: break;
            }

            has_normals = accessor_normal.IsValid()
                       || accessor_normal_rg16f.IsValid()
                       || accessor_normal_rg8un.IsValid()
                       || accessor_normal_rg8sn.IsValid()
                       || accessor_normal_a2rgb10un.IsValid()
                       || accessor_normal_a2bgr10un.IsValid();
        }

        vab = pc->GetVAB(VAN::Tangent);
        if(vab)
        {
            tangent_format = vab->GetFormat();

            switch(tangent_format)
            {
                case PF_RGB32F: accessor_tangent.Bind(vab, vertex_offset, vertex_count); break;
                case PF_RG16F: accessor_tangent_rg16f.Bind(vab, vertex_offset, vertex_count); break;
                case PF_RG8UN: accessor_tangent_rg8un.Bind(vab, vertex_offset, vertex_count); break;
                case PF_RG8SN: accessor_tangent_rg8sn.Bind(vab, vertex_offset, vertex_count); break;
                case PF_A2RGB10UN: accessor_tangent_a2rgb10un.Bind(vab, vertex_offset, vertex_count); break;
                case PF_A2BGR10UN: accessor_tangent_a2bgr10un.Bind(vab, vertex_offset, vertex_count); break;
                default: break;
            }

            has_tangents = accessor_tangent.IsValid()
                        || accessor_tangent_rg16f.IsValid()
                        || accessor_tangent_rg8un.IsValid()
                        || accessor_tangent_rg8sn.IsValid()
                        || accessor_tangent_a2rgb10un.IsValid()
                        || accessor_tangent_a2bgr10un.IsValid();
        }

        vab = pc->GetVAB(VAN::TexCoord);
        if(vab)
        {
            texcoord_format = vab->GetFormat();

            switch(texcoord_format)
            {
                case PF_RG32F: accessor_texcoord.Bind(vab, vertex_offset, vertex_count); break;
                case PF_RG16F: accessor_texcoord_rg16f.Bind(vab, vertex_offset, vertex_count); break;
                case PF_RG16UN: accessor_texcoord_rg16un.Bind(vab, vertex_offset, vertex_count); break;
                case PF_RG8UN: accessor_texcoord_rg8un.Bind(vab, vertex_offset, vertex_count); break;
                default: break;
            }

            has_texcoords = accessor_texcoord.IsValid()
                         || accessor_texcoord_rg16f.IsValid()
                         || accessor_texcoord_rg16un.IsValid()
                         || accessor_texcoord_rg8un.IsValid();
        }

        vab = pc->GetVAB(VAN::Color);
        if(vab)
        {
            color_format = vab->GetFormat();

            switch(color_format)
            {
                case PF_RGBA32F: accessor_color.Bind(vab, vertex_offset, vertex_count); break;
                case PF_RGBA16F: accessor_color_rgba16f.Bind(vab, vertex_offset, vertex_count); break;
                case PF_RGBA16UN: accessor_color_rgba16un.Bind(vab, vertex_offset, vertex_count); break;
                case PF_RGBA8UN: accessor_color_rgba8un.Bind(vab, vertex_offset, vertex_count); break;
                case PF_A2RGB10UN: accessor_color_a2rgb10un.Bind(vab, vertex_offset, vertex_count); break;
                case PF_A2BGR10UN: accessor_color_a2bgr10un.Bind(vab, vertex_offset, vertex_count); break;
                default: break;
            }

            has_colors = accessor_color.IsValid()
                      || accessor_color_rgba16f.IsValid()
                      || accessor_color_rgba16un.IsValid()
                      || accessor_color_rgba8un.IsValid()
                      || accessor_color_a2rgb10un.IsValid()
                      || accessor_color_a2bgr10un.IsValid();
        }

        vab = pc->GetVAB(VAN::Luminance);
        if(vab)
        {
            luminance_format = vab->GetFormat();

            switch(luminance_format)
            {
                case PF_R32F: accessor_luminance.Bind(vab, vertex_offset, vertex_count); break;
                case PF_R16F: accessor_luminance_r16f.Bind(vab, vertex_offset, vertex_count); break;
                case PF_R16UN: accessor_luminance_r16un.Bind(vab, vertex_offset, vertex_count); break;
                case PF_R8UN: accessor_luminance_r8un.Bind(vab, vertex_offset, vertex_count); break;
                default: break;
            }

            has_luminance = accessor_luminance.IsValid()
                         || accessor_luminance_r16f.IsValid()
                         || accessor_luminance_r16un.IsValid()
                         || accessor_luminance_r8un.IsValid();
        }
    }

    GeometryBuilder::~GeometryBuilder()
    {
        // BufferAccessor 自动管理生命周期，无需手动清理
    }
}
