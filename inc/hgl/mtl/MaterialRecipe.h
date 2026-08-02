#pragma once

#include <hgl/type/EnumUtil.h>
#include <hgl/common/CoordinateSystem.h>
#include <hgl/graph/PipelinePreset.h>
#include <hgl/graph/ssbo/SSBOTypes.h>
#include <hgl/util/hash/FNV1a.h>
#include <cstdint>
#include <string>
#include <vector>

namespace hgl::graph::mtl
{
    constexpr uint32_t InvalidBuiltinMaterialCreatorIDHint = 0xffffffffu;

    // 逻辑纹理槽位（与具体 descriptor set/binding 解耦）。
    // Resolve 阶段会把这些语义槽映射到 bindless handle + 运行时索引。
    // SSBO/slot/binding 基础类型已迁移到 <hgl/graph/ssbo/SSBOTypes.h>
    // Recipe 中的纹理绑定声明（纯输入，不包含任何运行时句柄）。
    struct RecipeTextureBinding
    {
        TextureSlot slot = TextureSlot::BaseColor; // 目标语义槽
        std::string resource_id;                   // 资源标识（路径/资产ID/逻辑名）
        uint32_t direct_value = 0;                // 直接写入 TextureLayerRow 的原始值（例如 array layer）
        bool use_direct_value = false;            // true 时忽略 resource_id，直接使用 direct_value
        bool required = false;                     // true 时缺失应触发显式错误
    };

    // Recipe 中的结构体绑定声明（告诉 Resolve 需要哪类参数结构）。
    struct RecipeStructBinding
    {
        uint32_t ssbo_slot = DefaultMaterialSSBOSlot; // DataIndex 行槽位
        SSBOType ssbo_type = SSBOType::UserDefined;        // 结构体所属 SSBO 类型（主字段）
        uint32_t ssbo_id = 0;                              // 结构体 SSBO 资源 ID（主字段，P1.55）
        uint32_t ssbo_element_index = 0;                         // 结构体行索引（默认 0；可通过 use_ssbo_element_index 显式重载）
        bool use_ssbo_element_index = false;                     // true 时使用 ssbo_element_index 覆盖默认索引
        bool shared_across_instances = false;              // true: 多实例共享同一结构体数据
    };

    struct RecipeSSBOAssetBinding
    {
        std::string ssbo_name;
        SSBOType ssbo_type = SSBOType::UserDefined;
        uint32_t ssbo_id = 0;
    };

    // 每个材质的 SSBO slot 声明（由 MaterialDefinition 显式列出）。
    // index 即 ssbo_slot；name 同时作为 GLSL 变量名与 C++ 绑定 key。
    struct MaterialSSBOSlotDecl
    {
        std::string name;           // GLSL 变量名 / C++ 绑定 key，如 "pbr_surface" / "pbr_surface_a"
        SSBOType    ssbo_type = SSBOType::UserDefined; // 数据结构语义
    };

    // BMI 来源标记：区分 built-in 硬编码实现与未来的文件化实现。
    enum class MaterialDefinitionSourceKind : uint8_t
    {
        BuiltIn = 0,  // M_* 硬编码 creator（用于 fallback 与少量保底材质）
        File,         // 外部 BMI 文件（未来主路径）
    };

    // BMI 用途标签：辅助调试 / 统计 / fallback 降级判断。
    enum class MaterialDefinitionUsageTag : uint8_t
    {
        General = 0,   // 普通材质
        Fallback,      // 错误/缺失材质保底
        Debug,         // 调试/编辑器专用
        Text,          // 文字渲染专用
        Sky,           // 天空专用
    };

    struct MaterialDefinition
    {
        // Part-A: 基础语义/元信息
        std::string definition_id;                                   // 正式主键（字符串 ID / 未来文件名）
        std::string definition_name;                                 // 人类可读名称
        uint32_t    builtin_creator_id = InvalidBuiltinMaterialCreatorIDHint;         // 当前阶段过渡字段（enum 序号）
        MaterialDefinitionSourceKind source_kind = MaterialDefinitionSourceKind::BuiltIn;         // 来源类型
        MaterialDefinitionUsageTag   usage_tag  = MaterialDefinitionUsageTag::General;            // 用途标签

        // Lod / 质量包络
        uint16_t default_lod   = 0;
        uint16_t lod_count     = 1;
        uint16_t quality_tier  = 0;

        // Part-B: 资源契约（当前阶段仅 SSBO；Texture/VAB 由 recipe 按需声明）
        std::vector<RecipeSSBOAssetBinding> required_ssbo_assets;

        // Part-B2: 材质 SSBO slot 显式声明（index == ssbo_slot）
        // name 用于 GLSL 变量名 与 C++ SetMaterialSSBOResource(name,...) 绑定。
        // 无 SSBO 的材质此列表为空。
        std::vector<MaterialSSBOSlotDecl> ssbo_slot_decls;

        // Part-C: request 构建参数（构建所需选项显式存储，
        //         不允许调用方根据名字/枚举范围做任何推断）
        bool is_2d            = false;  // 2D 材质
        bool is_text          = false;  // text 材质（优先级高于 is_2d）
        bool with_camera      = true;   // 3D 材质包含 Camera UBO
        bool with_local_to_world = true;// 包含 LocalToWorld 变换
        bool with_sky         = false;  // 包含 Sky/大气 UBO
        // 2D 专用（仅 is_2d=true 时有效）
        CoordinateSystem2D coordinate_system_2d = CoordinateSystem2D::NDC;
        bool local_to_world_2d = true;
    };

    // 纯声明式材质输入（不含 Vulkan/运行时句柄），是 MaterializationSpec 的上游输入。
    struct MaterialRecipe
    {
        std::string recipe_name;               // 配方名称（人类可读）
        std::string mtl_def_id;                // MaterialDefinition字符串主键（材质标识 / 未来文件名）
        std::string domain;                    // 资源/缓存域（用于隔离不同管线空间）
        CoordinateSystem2D coordinate_system_2d = CoordinateSystem2D::NDC; // 2D 材质坐标系作者意图
        bool local_to_world_2d = true;        // 2D 材质是否需要 L2W 变换
        uint16_t material_lod = 0;            // 作者层选择的材质 LOD
        uint16_t material_quality_tier = 0;   // 作者层质量层级（0 为默认）

        bool double_sided = false; // 双面渲染开关
        bool alpha_test = false;   // 是否启用 alpha test
        float alpha_cutoff = 0.5f; // alpha test 阈值（alpha < cutoff 丢弃）
        hgl::graph::PipelinePreset pipeline_preset = hgl::graph::PipelinePreset::Auto; // Auto: 按 MaterialDefinition 推导

        std::vector<RecipeTextureBinding> textures; // 所有纹理语义绑定
        std::vector<RecipeStructBinding> structs;   // 所有结构体语义绑定
        std::vector<RecipeSSBOAssetBinding> ssbo_assets; // 供最终 ShaderProgram 通过描述符名解析的 SSBO 资产 ID
    };

    inline const RecipeSSBOAssetBinding *FindRecipeSSBOAssetBinding(const MaterialRecipe &recipe,
                                                                    const char *ssbo_name,
                                                                    const SSBOType ssbo_type) noexcept
    {
        if (!ssbo_name || !*ssbo_name)
            return nullptr;

        for (const auto &asset : recipe.ssbo_assets)
        {
            if (asset.ssbo_name != ssbo_name)
                continue;

            if (asset.ssbo_type != SSBOType::UserDefined
             && ssbo_type != SSBOType::UserDefined
             && asset.ssbo_type != ssbo_type)
                continue;

            return &asset;
        }

        return nullptr;
    }

    inline void UpsertRecipeSSBOAssetBinding(MaterialRecipe &recipe,
                                             const std::string &ssbo_name,
                                             const SSBOType ssbo_type,
                                             const uint32_t ssbo_id)
    {
        if (ssbo_name.empty())
            return;

        for (auto &asset : recipe.ssbo_assets)
        {
            if (asset.ssbo_name != ssbo_name)
                continue;

            asset.ssbo_type = ssbo_type;
            asset.ssbo_id = ssbo_id;
            return;
        }

        RecipeSSBOAssetBinding asset{};
        asset.ssbo_name = ssbo_name;
        asset.ssbo_type = ssbo_type;
        asset.ssbo_id = ssbo_id;
        recipe.ssbo_assets.emplace_back(std::move(asset));
    }

    /**
     * CN: UpsertRecipeSSBOAssetBinding 的统一重载 —— 接受 SSBOBinding，
     *     无需将 type/id 分开传。配合 SSBOArrayAccessor::GetSSBOBinding() 使用：
     *       UpsertRecipeSSBOAssetBinding(recipe, name, accessor->GetSSBOBinding());
     * EN: Unified overload accepting SSBOBinding so type/id need not be passed separately.
     */
    inline void UpsertRecipeSSBOAssetBinding(MaterialRecipe &recipe,
                                             const std::string &ssbo_name,
                                             const SSBOBinding &binding)
    {
        UpsertRecipeSSBOAssetBinding(recipe, ssbo_name, binding.ssbo_type, binding.ssbo_id);
    }

    inline void ApplyBaseMaterialInfoDefaults(MaterialRecipe &recipe,
                                              const MaterialDefinition &bmi,
                                              const bool overwrite_existing = false)
    {
        if (recipe.mtl_def_id.empty())
            recipe.mtl_def_id = bmi.definition_id;

        // coordinate_system_2d/local_to_world_2d：保守策略，只在 overwrite 时复写。
        if (overwrite_existing)
        {
            recipe.coordinate_system_2d = bmi.coordinate_system_2d;
            recipe.local_to_world_2d = bmi.local_to_world_2d;
            recipe.material_quality_tier = bmi.quality_tier;
        }

        if (bmi.lod_count > 0 && recipe.material_lod >= bmi.lod_count)
            recipe.material_lod = bmi.default_lod;

        if (recipe.material_quality_tier == 0)
            recipe.material_quality_tier = bmi.quality_tier;

        for (const auto &asset : bmi.required_ssbo_assets)
            UpsertRecipeSSBOAssetBinding(recipe, asset.ssbo_name, asset.ssbo_type, asset.ssbo_id);
    }

    inline uint64_t HashMaterialRecipe(const MaterialRecipe &recipe) noexcept
    {
        uint64 hash = hgl::hash::FNV1aInit<uint64>();

        if (!recipe.recipe_name.empty())
            hash = hgl::hash::FNV1aAppendBytes(hash, recipe.recipe_name.data(), recipe.recipe_name.size());
        if (!recipe.mtl_def_id.empty())
            hash = hgl::hash::FNV1aAppendBytes(hash, recipe.mtl_def_id.data(), recipe.mtl_def_id.size());
        if (!recipe.domain.empty())
            hash = hgl::hash::FNV1aAppendBytes(hash, recipe.domain.data(), recipe.domain.size());
        hash = hgl::hash::FNV1aAppendValueBytes(hash, recipe.coordinate_system_2d);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, recipe.local_to_world_2d);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, recipe.material_lod);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, recipe.material_quality_tier);

        hash = hgl::hash::FNV1aAppendValueBytes(hash, recipe.double_sided);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, recipe.alpha_test);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, recipe.alpha_cutoff);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, recipe.pipeline_preset);

        const uint32_t texture_count = static_cast<uint32_t>(recipe.textures.size());
        hash = hgl::hash::FNV1aAppendValueBytes(hash, texture_count);
        for (const auto &texture : recipe.textures)
        {
            hash = hgl::hash::FNV1aAppendValueBytes(hash, texture.slot);
            if (!texture.resource_id.empty())
                hash = hgl::hash::FNV1aAppendBytes(hash, texture.resource_id.data(), texture.resource_id.size());
            hash = hgl::hash::FNV1aAppendValueBytes(hash, texture.direct_value);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, texture.use_direct_value);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, texture.required);
        }

        const uint32_t struct_count = static_cast<uint32_t>(recipe.structs.size());
        hash = hgl::hash::FNV1aAppendValueBytes(hash, struct_count);
        for (const auto &s : recipe.structs)
        {
            hash = hgl::hash::FNV1aAppendValueBytes(hash, s.ssbo_slot);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, s.ssbo_type);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, s.use_ssbo_element_index);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, s.shared_across_instances);
        }

        const uint32_t ssbo_asset_count = static_cast<uint32_t>(recipe.ssbo_assets.size());
        hash = hgl::hash::FNV1aAppendValueBytes(hash, ssbo_asset_count);
        for (const auto &asset : recipe.ssbo_assets)
        {
            if (!asset.ssbo_name.empty())
                hash = hgl::hash::FNV1aAppendBytes(hash, asset.ssbo_name.data(), asset.ssbo_name.size());
            hash = hgl::hash::FNV1aAppendValueBytes(hash, asset.ssbo_type);
        }

        return static_cast<uint64_t>(hash);
    }
}
