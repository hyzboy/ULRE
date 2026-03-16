/// NewDescriptorSetLayoutFactory.cpp — 新 4-Set 布局创建

#include<hgl/mtl/new/NewDescriptorSetLayoutFactory.h>
#include<hgl/mtl/new/DescriptorSetBindings.h>

namespace hgl::graph{

static VkDescriptorSetLayoutBinding MakeBinding(uint32_t binding, VkDescriptorType type, VkShaderStageFlags stages)
{
    VkDescriptorSetLayoutBinding b{};
    b.binding            = binding;
    b.descriptorType     = type;
    b.descriptorCount    = 1;
    b.stageFlags         = stages;
    b.pImmutableSamplers = nullptr;
    return b;
}

static VkDescriptorSetLayout CreateLayout(VkDevice device, const VkDescriptorSetLayoutBinding *bindings, uint32_t count)
{
    // 为每个绑定启用 PARTIALLY_BOUND，允许着色器未使用的绑定不必写入有效描述符
    VkDescriptorBindingFlags bind_flags[32]{};
    for(uint32_t i = 0; i < count && i < 32; ++i)
        bind_flags[i] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

    VkDescriptorSetLayoutBindingFlagsCreateInfo flags_ci{};
    flags_ci.sType          = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    flags_ci.bindingCount   = count;
    flags_ci.pBindingFlags  = bind_flags;

    VkDescriptorSetLayoutCreateInfo ci{};
    ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ci.pNext        = &flags_ci;
    ci.bindingCount = count;
    ci.pBindings    = bindings;

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;

    if(vkCreateDescriptorSetLayout(device, &ci, nullptr, &layout) != VK_SUCCESS)
        return VK_NULL_HANDLE;

    return layout;
}

namespace NewDescriptorSetLayoutFactory{

VkDescriptorSetLayout CreatePerSceneLayout(VkDevice device)
{
    const VkShaderStageFlags all_gfx = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    const VkDescriptorSetLayoutBinding bindings[] = {
        MakeBinding(DSBinding::PerScene::CameraInfo,   VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, all_gfx),
        MakeBinding(DSBinding::PerScene::SkyInfo,      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT),
        MakeBinding(DSBinding::PerScene::ViewportInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, all_gfx),
        MakeBinding(DSBinding::PerScene::LightBuffer,  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT),
    };

    return CreateLayout(device, bindings, sizeof(bindings)/sizeof(bindings[0]));
}

VkDescriptorSetLayout CreatePerViewLayout(VkDevice device)
{
    const VkDescriptorSetLayoutBinding bindings[] = {
        MakeBinding(DSBinding::PerView::LocalToWorld, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT),
    };

    return CreateLayout(device, bindings, sizeof(bindings)/sizeof(bindings[0]));
}

VkDescriptorSetLayout CreatePerMaterialLayout(VkDevice device, SurfaceType surface_type)
{
    // 基础绑定: MI SSBO + 6 标准纹理槽
    VkDescriptorSetLayoutBinding bindings[13]{};
    uint32_t count = 0;

    const VkShaderStageFlags frag = VK_SHADER_STAGE_FRAGMENT_BIT;

    bindings[count++] = MakeBinding(DSBinding::PerMaterial::MI_SSBO,     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,        frag);
    bindings[count++] = MakeBinding(DSBinding::PerMaterial::TexAlbedo,   VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frag);
    bindings[count++] = MakeBinding(DSBinding::PerMaterial::TexNormal,   VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frag);
    bindings[count++] = MakeBinding(DSBinding::PerMaterial::TexMR,       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frag);
    bindings[count++] = MakeBinding(DSBinding::PerMaterial::TexAO,       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frag);
    bindings[count++] = MakeBinding(DSBinding::PerMaterial::TexEmissive, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frag);
    bindings[count++] = MakeBinding(DSBinding::PerMaterial::TexDetail,   VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frag);

    // Special Surface 纹理扩展 (Skin/Hair/Cloth/Eye/ClearCoat/Water/Terrain 使用)
    if(surface_type != SurfaceType::Unlit && surface_type != SurfaceType::Standard)
    {
        bindings[count++] = MakeBinding(DSBinding::PerMaterial::TexSpecial0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frag);
        bindings[count++] = MakeBinding(DSBinding::PerMaterial::TexSpecial1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frag);
        bindings[count++] = MakeBinding(DSBinding::PerMaterial::TexSpecial2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frag);
        bindings[count++] = MakeBinding(DSBinding::PerMaterial::TexSpecial3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frag);
        bindings[count++] = MakeBinding(DSBinding::PerMaterial::TexSpecial4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frag);
        bindings[count++] = MakeBinding(DSBinding::PerMaterial::TexSpecial5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frag);
    }

    return CreateLayout(device, bindings, count);
}

VkDescriptorSetLayout CreatePerDrawLayout(VkDevice device, bool ssbo_platform)
{
    const VkShaderStageFlags frag    = VK_SHADER_STAGE_FRAGMENT_BIT;
    const VkShaderStageFlags all_gfx = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding bindings[22]{};
    uint32_t count = 0;

    bindings[count++] = MakeBinding(DSBinding::PerDraw::ColorPalette,     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         frag);
    bindings[count++] = MakeBinding(DSBinding::PerDraw::ShadowMapNear,    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frag);
    bindings[count++] = MakeBinding(DSBinding::PerDraw::ShadowMask,       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frag);
    bindings[count++] = MakeBinding(DSBinding::PerDraw::SSAO_RT,          VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frag);
    bindings[count++] = MakeBinding(DSBinding::PerDraw::IBL_Irradiance,   VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frag);
    bindings[count++] = MakeBinding(DSBinding::PerDraw::IBL_Prefiltered,  VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frag);
    bindings[count++] = MakeBinding(DSBinding::PerDraw::IBL_BRDF_LUT,    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frag);
    bindings[count++] = MakeBinding(DSBinding::PerDraw::SSS_LUT,         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frag);
    bindings[count++] = MakeBinding(DSBinding::PerDraw::DebugLightingCfg, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         frag);
    bindings[count++] = MakeBinding(DSBinding::PerDraw::HZB_Pyramid,     VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frag);
    bindings[count++] = MakeBinding(DSBinding::PerDraw::ClusterLightList, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         frag);
    bindings[count++] = MakeBinding(DSBinding::PerDraw::ClusterAABB,      VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         frag);
    bindings[count++] = MakeBinding(DSBinding::PerDraw::FogParams,        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         frag);
    bindings[count++] = MakeBinding(DSBinding::PerDraw::SSR_RT,           VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frag);
    bindings[count++] = MakeBinding(DSBinding::PerDraw::ExposureData,     VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         frag);
    bindings[count++] = MakeBinding(DSBinding::PerDraw::MeshletBuffer,    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         all_gfx);
    bindings[count++] = MakeBinding(DSBinding::PerDraw::InstanceBuffer,   VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         all_gfx);
    bindings[count++] = MakeBinding(DSBinding::PerDraw::TerrainHeightMap, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, all_gfx);

    if(ssbo_platform)
    {
        bindings[count++] = MakeBinding(DSBinding::PerDraw::VertexDataBuffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
        bindings[count++] = MakeBinding(DSBinding::PerDraw::IndexDataBuffer,  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
    }

    bindings[count++] = MakeBinding(DSBinding::PerDraw::ShadowMapCached,    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frag);
    bindings[count++] = MakeBinding(DSBinding::PerDraw::CapsuleShadowData,  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         frag);

    return CreateLayout(device, bindings, count);
}

NewPipelineLayoutData *CreateNewPipelineLayout(VkDevice device, SurfaceType surface_type, bool ssbo_platform)
{
    NewPipelineLayoutData *pld = new NewPipelineLayoutData();
    pld->device = device;

    pld->layouts[0] = CreatePerSceneLayout(device);
    pld->layouts[1] = CreatePerViewLayout(device);
    pld->layouts[2] = CreatePerMaterialLayout(device, surface_type);
    pld->layouts[3] = CreatePerDrawLayout(device, ssbo_platform);

    for(uint32_t i = 0; i < NEW_DS_COUNT; ++i)
    {
        if(pld->layouts[i] == VK_NULL_HANDLE)
        {
            delete pld;
            return nullptr;
        }
    }

    VkPipelineLayoutCreateInfo plci{};
    plci.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount = NEW_DS_COUNT;
    plci.pSetLayouts    = pld->layouts;

    if(vkCreatePipelineLayout(device, &plci, nullptr, &pld->pipeline_layout) != VK_SUCCESS)
    {
        delete pld;
        return nullptr;
    }

    return pld;
}

}//namespace NewDescriptorSetLayoutFactory

NewPipelineLayoutData::~NewPipelineLayoutData()
{
    if(device == VK_NULL_HANDLE)
        return;

    if(pipeline_layout != VK_NULL_HANDLE)
        vkDestroyPipelineLayout(device, pipeline_layout, nullptr);

    for(uint32_t i = 0; i < NEW_DS_COUNT; ++i)
        if(layouts[i] != VK_NULL_HANDLE)
            vkDestroyDescriptorSetLayout(device, layouts[i], nullptr);
}

}//namespace hgl::graph
