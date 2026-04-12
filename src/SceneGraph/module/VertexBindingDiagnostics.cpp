#include <hgl/graph/module/VertexBindingDiagnostics.h>
#include <hgl/common/VertexAttribDef.h>
#include <hgl/graph/geo/VKGeometry.h>
#include <hgl/mtl/MaterialAssetRecord.h>
#include <hgl/vk/VKFormat.h>
#include <hgl/vk/VKMaterialTemplate.h>
#include <hgl/vk/VKVertexAttribBuffer.h>
#include <hgl/vk/VKVertexInput.h>
#include <hgl/vk/VKVertexInputConfig.h>
#include <hgl/vk/VKVertexInputFormat.h>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace hgl::graph
{
namespace
{
static const char *SafePrefix(const char *prefix)
{
    return prefix ? prefix : "[VertexBindingDiag]";
}

static FILE *SafeFile(FILE *out)
{
    return out ? out : stderr;
}

static const char *SafeVertexAttribName(const VertexAttrib attrib)
{
    const char *name = GetVertexAttribName(attrib);
    return name ? name : "<unknown-attrib>";
}

static const char *SafeShaderTypeName(const VertexInputAttribute &via)
{
    const char *name = GetVertexAttribName(VABaseType(via.basetype), via.vec_size);
    return name ? name : "<unknown-shader-type>";
}

static const char *SafeFormatName(const VkFormat format)
{
    const char *name = GetVulkanFormatName(format);
    return name ? name : "VK_FORMAT_UNKNOWN";
}

static const char *InputRateName(const VkVertexInputRate rate)
{
    switch(rate)
    {
        case VK_VERTEX_INPUT_RATE_VERTEX:   return "vertex";
        case VK_VERTEX_INPUT_RATE_INSTANCE: return "instance";
        default:                            return "unknown";
    }
}
}

void DumpMaterialVertexInput(FILE *out,
                             const char *prefix,
                             const char *label,
                             const MaterialTemplate *material)
{
    FILE *fp = SafeFile(out);
    const char *tag = SafePrefix(prefix);
    const char *name = label ? label : "material.vertex_input";

    const VertexInput *vi = material ? material->GetVertexInput() : nullptr;
    if (!vi)
    {
        std::fprintf(fp, "%s   %s: <null>\n", tag, name);
        return;
    }

    const auto &via_array = vi->GetVIAArray();
    std::fprintf(fp, "%s   %s: count=%u\n", tag, name, via_array.count);

    for (uint i = 0; i < via_array.count; ++i)
    {
        const auto &via = via_array.items[i];
        std::fprintf(fp,
                     "%s     %s[%u]: attrib=%s shader_type=%s location=%u interpolation=%u\n",
                     tag,
                     name,
                     i,
                     SafeVertexAttribName(via.attrib),
                     SafeShaderTypeName(via),
                     static_cast<unsigned>(via.location),
                     static_cast<unsigned>(via.interpolation));
    }
}

void DumpVIL(FILE *out,
             const char *prefix,
             const char *label,
             const VIL *vil)
{
    FILE *fp = SafeFile(out);
    const char *tag = SafePrefix(prefix);
    const char *name = label ? label : "vil";

    if (!vil)
    {
        std::fprintf(fp, "%s   %s: <null>\n", tag, name);
        return;
    }

    const uint32_t count = vil->GetVertexAttribCount();
    std::fprintf(fp, "%s   %s: count=%u\n", tag, name, static_cast<unsigned>(count));

    for (uint32_t i = 0; i < count; ++i)
    {
        const VertexInputFormat *cfg = vil->GetConfig(i);
        if (!cfg)
            continue;

        std::fprintf(fp,
                     "%s     %s[%u]: attrib=%s format=%s stride=%u binding=%d input_rate=%s vec_size=%u\n",
                     tag,
                     name,
                     static_cast<unsigned>(i),
                     SafeVertexAttribName(cfg->attrib),
                     SafeFormatName(cfg->format),
                     static_cast<unsigned>(cfg->stride),
                     cfg->binding,
                     InputRateName(cfg->input_rate),
                     static_cast<unsigned>(cfg->vec_size));
    }
}

void DumpGeometryVertexFormats(FILE *out,
                               const char *prefix,
                               const char *label,
                               const Geometry *geometry,
                               bool include_vk_buffer)
{
    FILE *fp = SafeFile(out);
    const char *tag = SafePrefix(prefix);
    const char *name = label ? label : "geometry.vertex_formats";

    if (!geometry)
    {
        std::fprintf(fp, "%s   %s: <null geometry>\n", tag, name);
        return;
    }

    std::fprintf(fp,
                 "%s   %s: geometry='%s' vab_count=%u vertex_count=%llu\n",
                 tag,
                 name,
                 geometry->GetName().c_str(),
                 static_cast<unsigned>(geometry->GetVABCount()),
                 static_cast<unsigned long long>(geometry->GetVertexCount()));

    for (int i = 0; i < int(VAN::RANGE_SIZE); ++i)
    {
        const VertexAttrib attrib = VertexAttrib(i);
        const VAB *vab = geometry->GetVAB(attrib);
        if (!vab)
            continue;

        if (include_vk_buffer)
        {
            std::fprintf(fp,
                         "%s     %s[%d]: attrib=%s format=%s stride=%u count=%u total_bytes=%llu vk_buffer=%llu\n",
                         tag,
                         name,
                         i,
                         SafeVertexAttribName(attrib),
                         SafeFormatName(vab->GetFormat()),
                         static_cast<unsigned>(vab->GetStride()),
                         static_cast<unsigned>(vab->GetCount()),
                         static_cast<unsigned long long>(vab->GetTotalBytes()),
                         static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(vab->GetVkBuffer())));
        }
        else
        {
            std::fprintf(fp,
                         "%s     %s[%d]: attrib=%s format=%s stride=%u count=%u total_bytes=%llu\n",
                         tag,
                         name,
                         i,
                         SafeVertexAttribName(attrib),
                         SafeFormatName(vab->GetFormat()),
                         static_cast<unsigned>(vab->GetStride()),
                         static_cast<unsigned>(vab->GetCount()),
                         static_cast<unsigned long long>(vab->GetTotalBytes()));
        }
    }
}

void DumpVILConfig(FILE *out,
                   const char *prefix,
                   const char *label,
                   const VILConfig &cfg)
{
    FILE *fp = SafeFile(out);
    const char *tag = SafePrefix(prefix);
    const char *name = label ? label : "vil_config";

    std::vector<std::pair<VertexAttrib, VAConfig>> entries;
    entries.reserve(cfg.size());

    for (const auto &[attrib, value] : cfg)
        entries.emplace_back(attrib, value);

    std::sort(entries.begin(), entries.end(), [](const auto &lhs, const auto &rhs)
    {
        return static_cast<int>(lhs.first) < static_cast<int>(rhs.first);
    });

    std::fprintf(fp, "%s   %s: count=%zu\n", tag, name, entries.size());

    for (size_t i = 0; i < entries.size(); ++i)
    {
        const auto &[attrib, value] = entries[i];
        std::fprintf(fp,
                     "%s     %s[%zu]: attrib=%s format=%s input_rate=%s\n",
                     tag,
                     name,
                     i,
                     SafeVertexAttribName(attrib),
                     SafeFormatName(value.format),
                     InputRateName(value.input_rate));
    }
}

void DumpResolveVILIncompatibleDiagnostics(FILE *out,
                                           const char *prefix,
                                           MaterialTemplate *material,
                                           const Geometry *geometry,
                                           const mtl::MaterialAssetRecord &fallback_rec,
                                           const std::string &build_reason,
                                           const VILConfig &runtime_vil_cfg)
{
    FILE *fp = SafeFile(out);
    const char *tag = SafePrefix(prefix);

    std::fprintf(fp,
                 "%s ResolveVIL incompatible diagnostics begin: material='%s' geometry='%s' domain='%s' id='%s' reason='%s' prim=%u pipeline=%u\n",
                 tag,
                 material ? material->GetName().c_str() : "<null-material>",
                 geometry ? geometry->GetName().c_str() : "<null-geometry>",
                 fallback_rec.domain_id.c_str(),
                 fallback_rec.id.c_str(),
                 build_reason.c_str(),
                 static_cast<unsigned>(fallback_rec.prim),
                 static_cast<unsigned>(fallback_rec.pipeline));

    DumpMaterialVertexInput(fp, tag, "material.vertex_input", material);
    DumpVIL(fp, tag, "material.default_vil", material ? material->GetDefaultVIL() : nullptr);
    DumpGeometryVertexFormats(fp, tag, "geometry.vertex_format_map", geometry, false);
    DumpVILConfig(fp, tag, "runtime.vil_config", runtime_vil_cfg);

    std::fprintf(fp,
                 "%s ResolveVIL incompatible diagnostics end: material='%s' geometry='%s'\n",
                 tag,
                 material ? material->GetName().c_str() : "<null-material>",
                 geometry ? geometry->GetName().c_str() : "<null-geometry>");
}

void DumpPrimitiveBindingDiagnostics(FILE *out,
                                     const char *prefix,
                                     const Geometry *geometry,
                                     const VIL *vil,
                                     const std::string &material_name,
                                     const char *reason)
{
    FILE *fp = SafeFile(out);
    const char *tag = SafePrefix(prefix);

    std::fprintf(fp,
                 "%s reason=%s, material=%s, geometry=%s\n",
                 tag,
                 reason ? reason : "(unknown)",
                 material_name.c_str(),
                 geometry ? geometry->GetName().ToStdString().c_str() : "(null)");

    DumpVIL(fp, tag, "requested.vil", vil);
    DumpGeometryVertexFormats(fp, tag, "geometry.vab", geometry, true);
}
}