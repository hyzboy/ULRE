#pragma once

#include <hgl/CoreType.h>
#include <cstdint>

namespace hgl::graph
{
#pragma pack(push, 4)
    /**
     * RenderItemDescriptor - 16字节图元描述符 (4-ID)
     * 对应 GLSL 中的 uvec4 (std430 scalar/vec4 对齐)
     *
     * 4-ID 构成:
     * - transform_id: L2W 变换矩阵索引 (TransformDataStorage SSBO)
     * - geometry_id:  几何与绘制参数索引 (MeshDrawParams SSBO)
     * - material_id:  材质实例参数行索引 (MaterialData SSBO)
     * - texture_id:   材质纹理引用表索引 (TextureRef SSBO)
     */
    struct RenderItemDescriptor
    {
        uint32_t transform_id = 0;
        uint32_t geometry_id  = 0;
        uint32_t material_id  = 0;
        uint32_t texture_id   = 0;

        constexpr bool operator==(const RenderItemDescriptor &rhs) const noexcept
        {
            return transform_id == rhs.transform_id
                && geometry_id  == rhs.geometry_id
                && material_id  == rhs.material_id
                && texture_id   == rhs.texture_id;
        }

        constexpr bool operator!=(const RenderItemDescriptor &rhs) const noexcept
        {
            return !(*this == rhs);
        }
    };
#pragma pack(pop)

    static_assert(sizeof(RenderItemDescriptor) == 16, "RenderItemDescriptor must be exactly 16 bytes (uvec4 aligned)");

    using RenderItemHandle = uint32_t;
    constexpr RenderItemHandle INVALID_RENDER_ITEM_HANDLE = UINT32_MAX;
}
