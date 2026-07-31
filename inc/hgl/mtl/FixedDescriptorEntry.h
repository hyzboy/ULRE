#pragma once

#include<hgl/CoreType.h>
#include<hgl/mtl/DescriptorKind.h>
#include<hgl/mtl/DescriptorSemantic.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/common/DescriptorSetTypeDef.h>

namespace hgl::graph::mtl
{
    struct FixedDescriptorEntry
    {
        DescriptorSetType   set_type;
        DescriptorKind      kind;
        uint32_t            stage_flags;
        const char *        name;
        const char *        struct_name;
        const char *        glsl_type;
        DescriptorSemantic  semantic = DescriptorSemantic::Unknown;
        TextureSlot         texture_slot = TextureSlot::BaseColor;
        uint32_t            slot_index = GetMaterialStructSlotIndex(SSBOType::PBRSurface);
        SSBOType            ssbo_type = SSBOType::UserDefined;
        DescriptorSemanticLayer semantic_layer = DescriptorSemanticLayer::Unknown;
        uint32_t            ssbo_id = MakeRecipeSSBOId(0);
    };
}//namespace hgl::graph::mtl
