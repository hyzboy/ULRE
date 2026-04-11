#pragma once

#include <string>

#include <hgl/vk/VKMaterialTemplate.h>
#include <hgl/vk/VKVertexInput.h>
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

}
