#pragma once

#include <hgl/type/EnumUtil.h>
#include <hgl/util/hash/FNV1a.h>
#include <cstdint>
#include <string>
#include <vector>

namespace hgl::graph::mtl
{
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
        case SSBOType::UserDefined: return "UserDefined";
        default: return "Unknown";
        }
    }

    // 预留：后续可接入 C++/GLSL 结构版本硬校验。
    inline uint32_t GetSSBOTypeStructVersion(const SSBOType /*type*/) noexcept
    {
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
        bool required = false;                     // true 时缺失应触发显式错误
    };

    // Recipe 中的结构体绑定声明（告诉 Resolve 需要哪类参数结构）。
    struct RecipeStructBinding
    {
        DataSlot slot = DataSlot::PBRSurface;              // 目标数据语义槽
        SSBOType ssbo_type = SSBOType::UserDefined;        // 结构体所属 SSBO 类型（主字段）
        uint32_t ssbo_id = 0;                              // 结构体 SSBO 资源 ID（主字段，P1.55）
        std::string struct_name;                           // 过渡字段：旧路径结构名（后续移除）
        bool shared_across_instances = false;              // true: 多实例共享同一结构体数据
    };

    // 纯声明式材质输入（不含 Vulkan/运行时句柄），是 MaterializationSpec 的上游输入。
    struct MaterialRecipe
    {
        std::string recipe_name;   // 配方名称（人类可读）
        std::string shading_model; // 着色模型标识（如 "PBR"、"Unlit"）
        std::string domain;        // 资源/缓存域（用于隔离不同管线空间）

        bool double_sided = false; // 双面渲染开关
        bool alpha_test = false;   // 是否启用 alpha test
        float alpha_cutoff = 0.5f; // alpha test 阈值（alpha < cutoff 丢弃）

        std::vector<RecipeTextureBinding> textures; // 所有纹理语义绑定
        std::vector<RecipeStructBinding> structs;   // 所有结构体语义绑定
    };

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
        if (!recipe.shading_model.empty())
            hash = hgl::hash::FNV1aAppendBytes(hash, recipe.shading_model.data(), recipe.shading_model.size());
        if (!recipe.domain.empty())
            hash = hgl::hash::FNV1aAppendBytes(hash, recipe.domain.data(), recipe.domain.size());

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
            hash = hgl::hash::FNV1aAppendValueBytes(hash, texture.required);
        }

        const uint32_t struct_count = static_cast<uint32_t>(recipe.structs.size());
        hash = hgl::hash::FNV1aAppendValueBytes(hash, struct_count);
        for (const auto &s : recipe.structs)
        {
            hash = hgl::hash::FNV1aAppendValueBytes(hash, s.slot);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, s.ssbo_type);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, s.ssbo_id);
            if (!s.struct_name.empty())
                hash = hgl::hash::FNV1aAppendBytes(hash, s.struct_name.data(), s.struct_name.size());
            hash = hgl::hash::FNV1aAppendValueBytes(hash, s.shared_across_instances);
        }

        return static_cast<uint64_t>(hash);
    }
}
