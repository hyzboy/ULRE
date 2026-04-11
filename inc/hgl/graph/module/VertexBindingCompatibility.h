#pragma once

#include <string>

#include <hgl/vk/VKMaterialTemplate.h>
#include <hgl/vk/VKVertexInput.h>
#include <hgl/vk/VKVertexInputConfig.h>
#include <hgl/vk/VKVertexAttribBuffer.h>
#include <hgl/graph/geo/VKGeometry.h>
#include <hgl/mtl/VertexAttributeSpec.h>

namespace hgl::graph
{

inline const VertexInputAttribute *FindMaterialVIAByAttrib(const VertexInput *vi,const VertexAttrib attrib)
{
    if(!vi)
        return nullptr;

    const auto &via_array = vi->GetVIAArray();
    const VertexInputAttribute *via = via_array.items;

    for(uint i = 0; i < via_array.count; ++i)
    {
        if(via->attrib == attrib)
            return via;

        ++via;
    }

    return nullptr;
}

inline bool IsMaterialVertexAttribStorageCompatible(const VertexInput *vi,
                                                    const VertexAttrib attrib,
                                                    const VkFormat storage_format,
                                                    std::string *reason = nullptr)
{
    const VertexInputAttribute *mat_via = FindMaterialVIAByAttrib(vi, attrib);
    if(!mat_via)
    {
        if(reason)
            *reason = "material_vertex_input_missing_attrib";
        return false;
    }

    VAType shader_type;
    shader_type.basetype = VABaseType(mat_via->basetype);
    shader_type.vec_size = mat_via->vec_size;

    if(!mtl::IsStorageFormatCompatibleWithShaderType(shader_type, storage_format))
    {
        if(reason)
            *reason = "shader_storage_incompatible";
        return false;
    }

    return true;
}

inline bool IsMaterialStorageCompatible(const MaterialTemplate *material,
                                        const VertexAttrib attrib,
                                        const VkFormat storage_format,
                                        std::string *reason = nullptr)
{
    if(!material)
    {
        if(reason)
            *reason = "material_missing";
        return false;
    }

    return IsMaterialVertexAttribStorageCompatible(material->GetVertexInput(), attrib, storage_format, reason);
}

inline bool BuildGeometryDrivenVILConfig(const MaterialTemplate *material,
                                         const Geometry *geometry,
                                         const VIL *requested_vil,
                                         VILConfig &out_cfg,
                                         bool &has_any,
                                         std::string *reason = nullptr,
                                         bool *has_layout_mismatch = nullptr)
{
    out_cfg.clear();
    has_any = false;
    if(has_layout_mismatch)
        *has_layout_mismatch = false;

    if(!material)
    {
        if(reason)
            *reason = "material_missing";
        return false;
    }

    if(!geometry)
    {
        if(reason)
            *reason = "geometry_missing";
        return false;
    }

    auto add_attrib = [&](const VertexAttrib attrib,
                          const VkVertexInputRate input_rate,
                          const VkFormat requested_format,
                          const uint32_t requested_stride,
                          const bool has_requested_layout) -> bool
    {
        auto *vab = geometry->GetVAB(attrib);
        if(!vab)
        {
            if(has_requested_layout)
            {
                if(reason)
                    *reason = "vab_missing_for_required_attrib";
                return false;
            }

            return true;
        }

        std::string compat_reason;
        if(!IsMaterialStorageCompatible(material, attrib, vab->GetFormat(), &compat_reason))
        {
            if(reason)
                *reason = compat_reason;
            return false;
        }

        has_any = true;

        if(!out_cfg.Add(attrib, VAConfig(vab->GetFormat(), input_rate)))
        {
            if(reason)
                *reason = "runtime_vil_config_add_failed";
            return false;
        }

        if(has_layout_mismatch && has_requested_layout)
        {
            if(vab->GetFormat() != requested_format || vab->GetStride() != requested_stride)
                *has_layout_mismatch = true;
        }

        return true;
    };

    if(requested_vil)
    {
        const uint32_t input_count = requested_vil->GetVertexAttribCount();
        const VertexInputFormat *vif = requested_vil->GetVIFList();

        for(uint32_t i = 0; i < input_count; ++i)
        {
            if(!add_attrib(vif->attrib, vif->input_rate, vif->format, vif->stride, true))
                return false;

            ++vif;
        }
    }
    else
    {
        for(int i = 0; i < int(VAN::RANGE_SIZE); ++i)
        {
            const VertexAttrib attrib = VertexAttrib(i);
            if(!add_attrib(attrib, VK_VERTEX_INPUT_RATE_VERTEX, VK_FORMAT_UNDEFINED, 0, false))
                return false;
        }
    }

    return true;
}

}
