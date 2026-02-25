#include <string>
/// MaterialCompiler.cpp — FixedMaterialDef → MaterialCreateInfo 编译器实现
///
/// 三步流程：
///   1. 从 FixedDescriptorEntry[] 构建 MaterialDescriptorInfo（描述符布局）
///   2. 从 FixedVertexEntry[] 设置顶点输入
///   3. 使用 ShaderPermutationKey 的宏前缀编译 GLSL + 设置 MI 代码

#include <hgl/graph/mtl/MaterialCompiler.h>
#include <hgl/graph/mtl/Material3DCreateConfig.h>
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/shadergen/ShaderDescriptorInfo.h>
#include <hgl/shadergen/ShaderCreateInfoVertex.h>
#include <hgl/shadergen/ShaderCreateInfoFragment.h>
#include <hgl/graph/mtl/UBOCommon.h>
#include <hgl/vk/VKDeviceAttribute.h>
#include <cstring>

namespace hgl::graph::mtl {

/**
 * 编译一个 FixedMaterialDef 排列到 MaterialCreateInfo。
 *
 * 内部流程：
 *   1. 若 def 中 descriptor_entries 为空，不添加任何 UBO/SSBO/Texture
 *   2. 从 FixedVertexEntry[] 设置顶点输入
 *   3. 生成排列宏前缀 + GLSL 源码，编译到 SPV
 *   4. 若 def.mi_glsl_codes 非空，设置 MaterialInstance
 */
MaterialCreateInfo *CompileFixedMaterial(
    const VulkanDevAttr *       dev_attr,
    const FixedMaterialDef &    def,
    const ShaderPermutationKey &key)
{
    if (!dev_attr)
        return nullptr;

    // ─────────────────────────────────────────────────────────────────────────
    // Step 1: 创建 MaterialCreateConfig（最小化配置）
    // ─────────────────────────────────────────────────────────────────────────

    Material3DCreateConfig cfg;
    cfg.prim = def.primitive_type;
    cfg.shader_stage_flag_bit = uint32_t(ShaderStage::VertexFragment);
    if (def.geom_glsl) 
        cfg.shader_stage_flag_bit |= uint32_t(ShaderStage::Geometry);

    // ─────────────────────────────────────────────────────────────────────────
    // Step 2: 创建 MaterialCreateInfo
    // ─────────────────────────────────────────────────────────────────────────

    MaterialCreateInfo *mci = new MaterialCreateInfo(&cfg);
    mci->SetDevice(dev_attr);

    // ─────────────────────────────────────────────────────────────────────────
    // Step 3: 从 FixedDescriptorEntry[] 添加描述符
    // ─────────────────────────────────────────────────────────────────────────

    for (uint32_t i = 0; i < def.descriptor_entry_count; ++i)
    {
        const FixedDescriptorEntry &entry = def.descriptor_entries[i];
        const uint32_t stage_bits = entry.stage_flags;
        const DescriptorSetType set_type = entry.set_type;

        switch (entry.kind)
        {
        case DescriptorKind::UBO:
            if (entry.struct_name)
            {
                // 必须先添加结构体定义
                mci->AddStruct(std::string(entry.struct_name),
                              std::string());  // GLSL 代码由 shader 文件本身提供，此处留空
                mci->AddUBO(stage_bits, set_type,
                           std::string(entry.struct_name), std::string(entry.name));
            }
            break;

        case DescriptorKind::SSBO:
            if (entry.struct_name)
            {
                mci->AddStruct(std::string(entry.struct_name),
                              std::string());
                mci->AddSSBO(stage_bits, set_type,
                            std::string(entry.struct_name), std::string(entry.name));
            }
            break;

        case DescriptorKind::Texture:
            // Texture 不需要 struct_name
            if (entry.glsl_type)
            {
                TextureType tt;
                const char *glsl_type_str = entry.glsl_type;
                
                // 常见类型映射
                if (strcmp(glsl_type_str, "sampler2D") == 0)
                    tt = TextureType::Texture2D;
                else if (strcmp(glsl_type_str, "sampler3D") == 0)
                    tt = TextureType::Texture3D;
                else if (strcmp(glsl_type_str, "samplerCube") == 0)
                    tt = TextureType::TextureCube;
                else if (strcmp(glsl_type_str, "sampler2DArray") == 0)
                    tt = TextureType::Texture2DArray;
                else
                    tt = TextureType::Texture2D;  // fallback

                mci->AddTexture(ShaderStage(stage_bits), set_type, tt,
                               std::string(entry.name));
            }
            break;

        case DescriptorKind::TextureSampler:
            if (entry.glsl_type)
            {
                TextureType tt;
                SamplerType st = SamplerType::Sampler2D; // 默认
                const char *glsl_type_str = entry.glsl_type;

                if (strcmp(glsl_type_str, "sampler2D") == 0) {
                    tt = TextureType::Texture2D;
                    st = SamplerType::Sampler2D;
                } else if (strcmp(glsl_type_str, "samplerCube") == 0) {
                    tt = TextureType::TextureCube;
                    st = SamplerType::SamplerCube;
                } else if (strcmp(glsl_type_str, "sampler2DArray") == 0) {
                    tt = TextureType::Texture2DArray;
                    st = SamplerType::Sampler2DArray;
                } else {
                    tt = TextureType::Texture2D;
                    st = SamplerType::Sampler2D;
                }

                mci->AddTextureSampler(ShaderStage(stage_bits), set_type, st, std::string(entry.name));
            }
            break;
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Step 4: 从 FixedVertexEntry[] 添加顶点输入
    // ─────────────────────────────────────────────────────────────────────────

    ShaderCreateInfoVertex *vsc = mci->GetVS();
    if (vsc)
    {
        for (uint32_t i = 0; i < def.vertex_entry_count; ++i)
        {
            const FixedVertexEntry &entry = def.vertex_entries[i];
            vsc->AddInput(entry.type, entry.name);
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Step 5: 设置 MaterialInstance（若有）
    // ─────────────────────────────────────────────────────────────────────────

    if (def.mi_glsl_codes && def.mi_struct_bytes > 0)
    {
        mci->SetMaterialInstance(
            std::string(def.mi_glsl_codes),
            def.mi_struct_bytes,
            uint32_t(ShaderStage::Vertex));  // MI 数据通常在 VS 中使用
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Step 6: 编译 GLSL → SPV
    //
    // 流程：
    //   1. 生成排列宏前缀：key.AppendGLSLDefines(prefix)
    //   2. prefix + def.vert_glsl/frag_glsl → 完整 GLSL
    //   3. ShaderCreateInfo 进行 glslang 编译到 SPV
    // ─────────────────────────────────────────────────────────────────────────

    std::string glsl_prefix;
    key.AppendGLSLDefines(glsl_prefix);

    // 设置 shaders（这里调用 mci 的 shader 编译接口）
    ShaderCreateInfoVertex *vert = mci->GetVS();
    ShaderCreateInfoFragment *frag = mci->GetFS();

    // 暂时使用 SetMain() 的方式来设置源码（不完美，但能工作）
    if (vert && def.vert_glsl)
    {
        std::string vert_source = glsl_prefix + def.vert_glsl;
        vert->SetMain(vert_source.c_str(), vert_source.length());
    }

    if (frag && def.frag_glsl)
    {
        std::string frag_source = glsl_prefix + def.frag_glsl;
        frag->SetMain(frag_source.c_str(), frag_source.length());
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Step 7: 调用 CreateShader() 编译到 SPV
    // ─────────────────────────────────────────────────────────────────────────

    if (!mci->CreateShader())
    {
        delete mci;
        return nullptr;
    }

    return mci;
}

}  // namespace hgl::graph::mtl
