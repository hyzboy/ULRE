#pragma once

#include <hgl/type/EnumUtil.h>
#include <hgl/common/CoordinateSystem.h>
#include <hgl/util/hash/FNV1a.h>
#include <cstdint>
#include <string>
#include <vector>

namespace hgl::graph::mtl
{
    // 材质语义层：渲染技术模型（作者层主入口）。
    enum class ShadingModel : uint8_t
    {
        Unknown = 0,
        Unlit,
        Standard,
        Sky,
        Text,
        Legacy,
        Custom,

        ENUM_CLASS_RANGE(Unknown, Custom)
    };

    constexpr uint32_t InvalidMaterialPresetHint = 0xffffffffu;

    // 逻辑纹理槽位（与具体 descriptor set/binding 解耦）。
    // Resolve 阶段会把这些语义槽映射到 bindless handle + 运行时索引。
    enum class TextureSlot : uint8_t
    {
        BaseColor = 0,   // Albedo / BaseColor 颜色纹理
        Normal,          // 法线纹理（通常 tangent-space）
        Metallic,        // 金属度纹理（已与 Roughness 拆分）
        Roughness,       // 粗糙度纹理（已与 Metallic 拆分）
        Emissive,        // 自发光纹理
        Occlusion,       // AO 纹理
        OpacityMask,     // Mask/AlphaTest 纹理
        Height,          // Height/Parallax 纹理
        Custom0,         // 预留自定义槽
        Custom1,         // 预留自定义槽

        ENUM_CLASS_RANGE(BaseColor, Custom1)
    };

    // 逻辑结构体数据槽位（用于声明“实例参数属于哪一类数据语义”）。
    enum class DataSlot : uint8_t
    {
        PBRSurface = 0,      // 基础 PBR 参数块（baseColor/metallic/roughness 等）
        EmissiveSurface,     // 发光参数块
        ClearCoatSurface,    // ClearCoat 参数块
        TransmissionSurface, // 透射/折射参数块
        User0,               // 预留自定义槽
        User1,               // 预留自定义槽

        ENUM_CLASS_RANGE(PBRSurface, User1)
    };

    // SSBO 类型枚举：用于在 Recipe/Spec 中以稳定整数传递“结构体数据落在哪类缓冲”。
    enum class SSBOType : uint16_t
    {
        TextureLayer = 0,    // 纹理层/句柄索引表
        DataIndex,           // 间接数据索引表（实例 -> 结构体索引）
        PBRSurface,          // PBRSurface 结构体池
        EmissiveSurface,     // EmissiveSurface 结构体池
        ClearCoatSurface,    // ClearCoatSurface 结构体池
        TransmissionSurface, // TransmissionSurface 结构体池
        TransformIndexRows,  // TransformID 行表（实例/分组 -> L2W 索引）
        LocalToWorld,        // LocalToWorld 矩阵池
        UserDefined,         // 预留扩展池

        ENUM_CLASS_RANGE(TextureLayer, UserDefined)
    };

    // 兼容旧命名（过渡期保留）。
    using SSBOCategory = SSBOType;

    // SSBOType -> 稳定名称映射（供 ShaderGen/GLSL 文件定位使用）。
    inline const char *GetSSBOTypeName(const SSBOType type) noexcept
    {
        switch (type)
        {
        case SSBOType::TextureLayer: return "TextureLayer";
        case SSBOType::DataIndex: return "DataIndex";
        case SSBOType::PBRSurface: return "PBRSurface";
        case SSBOType::EmissiveSurface: return "EmissiveSurface";
        case SSBOType::ClearCoatSurface: return "ClearCoatSurface";
        case SSBOType::TransmissionSurface: return "TransmissionSurface";
        case SSBOType::TransformIndexRows: return "TransformIndexRows";
        case SSBOType::LocalToWorld: return "LocalToWorld";
        case SSBOType::UserDefined: return "UserDefined";
        default: return "Unknown";
        }
    }

    // SSBO 结构版本（R11）：仅对已冻结布局类型返回非 0 版本号。
    // 返回 0 表示该类型尚未纳入硬校验（例如用户自定义/可变布局类型）。
    inline uint32_t GetSSBOTypeStructVersion(const SSBOType type) noexcept
    {
        switch (type)
        {
        case SSBOType::TextureLayer:
        case SSBOType::DataIndex:
        case SSBOType::TransformIndexRows:
        case SSBOType::LocalToWorld:
            return 1;
        default:
            break;
        }

        return 0;
    }

    // SSBO 类型对应的冻结字节步长（R11）。
    // 返回 0 表示当前类型没有固定步长约束。
    inline uint32_t GetSSBOTypeStructStride(const SSBOType type) noexcept
    {
        switch (type)
        {
        case SSBOType::TextureLayer:
            return sizeof(uint32_t) * static_cast<uint32_t>(TextureSlot::RANGE_SIZE);
        case SSBOType::DataIndex:
            return sizeof(uint32_t) * static_cast<uint32_t>(DataSlot::RANGE_SIZE);
        case SSBOType::TransformIndexRows:
            return sizeof(uint32_t);
        case SSBOType::LocalToWorld:
            return sizeof(float) * 16;
        default:
            break;
        }

        return 0;
    }

    // SSBO ID 命名空间约定（P1.55-01 冻结）：
    // - 最高位=0：Recipe/资产侧可分配；
    // - 最高位=1：ECS 内生通道保留（如 TransformDataStorage 产物）。
    constexpr uint32_t SSBOIdNamespaceBit = 0x80000000u;
    constexpr uint32_t SSBOIdLocalMask = 0x7fffffffu;

    constexpr uint32_t MakeRecipeSSBOId(const uint32_t local_id) noexcept
    {
        return local_id & SSBOIdLocalMask;
    }

    constexpr uint32_t MakeECSSSBOId(const uint32_t local_id) noexcept
    {
        return (local_id & SSBOIdLocalMask) | SSBOIdNamespaceBit;
    }

    constexpr bool IsECSSSBOId(const uint32_t ssbo_id) noexcept
    {
        return (ssbo_id & SSBOIdNamespaceBit) != 0;
    }

    constexpr uint32_t GetSSBOIdLocalPart(const uint32_t ssbo_id) noexcept
    {
        return ssbo_id & SSBOIdLocalMask;
    }

    namespace ECSReservedSSBOId
    {
        // Transform 通道（ECS 内生，来自 TransformDataStorage）
        constexpr uint32_t TransformIndexRows   = MakeECSSSBOId(1);
        constexpr uint32_t LocalToWorldData     = MakeECSSSBOId(2);

        // Material 通道（ECS 生产，供 recipe/材质按需消费）
        constexpr uint32_t MaterialInstanceRows = MakeECSSSBOId(3);
        constexpr uint32_t MaterialInstanceData = MakeECSSSBOId(4);
        constexpr uint32_t TextureLayerRows     = MakeECSSSBOId(5);
    }

    struct SSBOAddress
    {
        SSBOType ssbo_type = SSBOType::UserDefined;
        uint32_t ssbo_id = 0;
        uint32_t slot = 0;
    };

    inline SSBOAddress MakeSSBOAddress(const SSBOType ssbo_type, const uint32_t ssbo_id, const DataSlot slot) noexcept
    {
        return SSBOAddress{ssbo_type, ssbo_id, static_cast<uint32_t>(slot)};
    }

    inline SSBOAddress MakeSSBOAddress(const SSBOType ssbo_type, const uint32_t ssbo_id, const TextureSlot slot) noexcept
    {
        return SSBOAddress{ssbo_type, ssbo_id, static_cast<uint32_t>(slot)};
    }

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
        DataSlot slot = DataSlot::PBRSurface;              // 目标数据语义槽
        SSBOType ssbo_type = SSBOType::UserDefined;        // 结构体所属 SSBO 类型（主字段）
        uint32_t ssbo_id = 0;                              // 结构体 SSBO 资源 ID（主字段，P1.55）
        uint32_t struct_index = 0;                         // 结构体行索引（默认 0；可通过 use_struct_index 显式重载）
        bool use_struct_index = false;                     // true 时使用 struct_index 覆盖默认索引
        bool shared_across_instances = false;              // true: 多实例共享同一结构体数据
    };

    struct RecipeSSBOAssetBinding
    {
        std::string ssbo_name;
        SSBOType ssbo_type = SSBOType::UserDefined;
        uint32_t ssbo_id = 0;
    };

    struct BaseMaterialInfo
    {
        std::string bmi_name;
        ShadingModel shading_model = ShadingModel::Unknown;
        uint32_t preset_hint = InvalidMaterialPresetHint;

        // 2D authoring defaults
        CoordinateSystem2D coordinate_system_2d = CoordinateSystem2D::NDC;
        bool local_to_world_2d = true;

        // Variant/Lod authoring envelope (base definition >= concrete program usage)
        uint16_t default_lod = 0;
        uint16_t lod_count = 1;
        uint16_t quality_tier = 0;

        // Required semantic assets for this base material definition.
        std::vector<RecipeSSBOAssetBinding> required_ssbo_assets;
    };

    // 纯声明式材质输入（不含 Vulkan/运行时句柄），是 MaterializationSpec 的上游输入。
    struct MaterialRecipe
    {
        std::string recipe_name;               // 配方名称（人类可读）
        ShadingModel shading_model = ShadingModel::Unknown; // 着色模型语义
        uint32_t preset_hint = InvalidMaterialPresetHint;   // 过渡期提示（MaterialPreset 序号）
        std::string base_material_info_name;   // 作者层基材质定义名（BMI）
        std::string domain;                    // 资源/缓存域（用于隔离不同管线空间）
        CoordinateSystem2D coordinate_system_2d = CoordinateSystem2D::NDC; // 2D 材质坐标系作者意图
        bool local_to_world_2d = true;        // 2D 材质是否需要 L2W 变换
        uint16_t material_lod = 0;            // 作者层选择的材质 LOD
        uint16_t material_quality_tier = 0;   // 作者层质量层级（0 为默认）

        bool double_sided = false; // 双面渲染开关
        bool alpha_test = false;   // 是否启用 alpha test
        float alpha_cutoff = 0.5f; // alpha test 阈值（alpha < cutoff 丢弃）

        std::vector<RecipeTextureBinding> textures; // 所有纹理语义绑定
        std::vector<RecipeStructBinding> structs;   // 所有结构体语义绑定
        std::vector<RecipeSSBOAssetBinding> ssbo_assets; // 供最终 MaterialProgram 通过描述符名解析的 SSBO 资产 ID
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

    inline void ApplyBaseMaterialInfoDefaults(MaterialRecipe &recipe,
                                              const BaseMaterialInfo &bmi,
                                              const bool overwrite_existing = false)
    {
        if (recipe.base_material_info_name.empty())
            recipe.base_material_info_name = bmi.bmi_name;

        if (recipe.shading_model == ShadingModel::Unknown || overwrite_existing)
            recipe.shading_model = bmi.shading_model;

        if (recipe.preset_hint == InvalidMaterialPresetHint || overwrite_existing)
            recipe.preset_hint = bmi.preset_hint;

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

    // 计算 MaterialRecipe 的稳定内容哈希（只看声明内容，不依赖运行时句柄）。
    // 该哈希可用于：
    // 1) Recipe 去重/缓存；
    // 2) MaterializationSpec::recipe_hash 的来源值；
    // 3) “同输入应产出同 spec”的一致性校验。
    inline uint64_t HashMaterialRecipe(const MaterialRecipe &recipe) noexcept
    {
        uint64 hash = hgl::hash::FNV1aInit<uint64>();

        if (!recipe.recipe_name.empty())
            hash = hgl::hash::FNV1aAppendBytes(hash, recipe.recipe_name.data(), recipe.recipe_name.size());
        hash = hgl::hash::FNV1aAppendValueBytes(hash, recipe.shading_model);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, recipe.preset_hint);
        if (!recipe.base_material_info_name.empty())
            hash = hgl::hash::FNV1aAppendBytes(hash, recipe.base_material_info_name.data(), recipe.base_material_info_name.size());
        if (!recipe.domain.empty())
            hash = hgl::hash::FNV1aAppendBytes(hash, recipe.domain.data(), recipe.domain.size());
        hash = hgl::hash::FNV1aAppendValueBytes(hash, recipe.coordinate_system_2d);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, recipe.local_to_world_2d);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, recipe.material_lod);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, recipe.material_quality_tier);

        hash = hgl::hash::FNV1aAppendValueBytes(hash, recipe.double_sided);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, recipe.alpha_test);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, recipe.alpha_cutoff);

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
            hash = hgl::hash::FNV1aAppendValueBytes(hash, s.slot);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, s.ssbo_type);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, s.ssbo_id);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, s.struct_index);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, s.use_struct_index);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, s.shared_across_instances);
        }

        const uint32_t ssbo_asset_count = static_cast<uint32_t>(recipe.ssbo_assets.size());
        hash = hgl::hash::FNV1aAppendValueBytes(hash, ssbo_asset_count);
        for (const auto &asset : recipe.ssbo_assets)
        {
            if (!asset.ssbo_name.empty())
                hash = hgl::hash::FNV1aAppendBytes(hash, asset.ssbo_name.data(), asset.ssbo_name.size());
            hash = hgl::hash::FNV1aAppendValueBytes(hash, asset.ssbo_type);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, asset.ssbo_id);
        }

        return static_cast<uint64_t>(hash);
    }
}
