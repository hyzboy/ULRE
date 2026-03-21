#pragma once

#include<hgl/common/DescriptorSetTypeDef.h>
#include<hgl/mtl/SamplerName.h>
#include<vector>
#include<string>
#include<cstring>

namespace hgl::graph::mtl
{
    enum class DescriptorKind : uint8
    {
        UBO,
        SSBO,
        Texture,
        TextureSampler,
    };

    enum class DescriptorSemantic : uint8
    {
        Unknown = 0,

        ViewportInfo,
        CameraInfo,
        SkyInfo,

        TransformID,
        LocalToWorld,
        MaterialInstanceID,
        MaterialInstance,

        MaterialInstanceTextureID,        ///<当使用TextureArray时的材质ID信息
        //比如一个材质有BaseColor,Normal两个纹理，但是开发者配置了使用TextureArray
        //那么原本的纹理的Sampler2D TexBaseColor;就会变成Sampler2DArray TexBaseColor;
        //然后 glsl 中会出现名为 MaterialInstanceTexture 的结构，结构里面有 uint BaseColor; uint Normal;这样的成员，表示当前材质实例使用的纹理ID（TextureArray的Layer index）。如果后续有其它类型的纹理，也会自动扩展这个结构体，增加成员。
        //再之后会出现名为 MaterialInstanceTextureID 的SSBO, 这的结构内部是MaterialInstanceTexture tex_id[];
        // 后面要访问纹理内容的，全部根据材质ID，从MaterialInstanceTextureID SSBO里取出对应的纹理ID，再去访问TextureArray。这样就实现了在不增加DrawCall的前提下，单个材质实例使用不同纹理的功能。

        //MaterialSampler,  //未启用

        ColorPattle,        ///<调色板(目前仅Line绘制使用)

        BoneJoint,          ///<骨骼节点ID
        BoneJointWeight,    ///<骨骼权重

        Custom,
    };

    struct FixedDescriptorEntry
    {
        DescriptorSetType   set_type;
        DescriptorKind      kind;
        uint32_t            stage_flags;
        const char *        name;
        const char *        struct_name;
        const char *        glsl_type;
        DescriptorSemantic  semantic = DescriptorSemantic::Unknown;
    };

    struct DescriptorRequirement
    {
        DescriptorSemantic semantic = DescriptorSemantic::Unknown;
        DescriptorKind kind = DescriptorKind::UBO;
        uint32_t stage_flags = 0;
        const char *name_override = nullptr;
    };

    struct ResolvedDescriptorRequirement
    {
        DescriptorSemantic semantic = DescriptorSemantic::Unknown;
        DescriptorSetType set_type = DescriptorSetType::Unknow;
        DescriptorKind kind = DescriptorKind::UBO;
        uint32_t stage_flags = 0;

        const char *name = nullptr;
        const char *struct_name = nullptr;
        const char *glsl_type = nullptr;

        bool required = true;
        bool allow_fallback = false;
    };

    struct BindingContract
    {
        std::vector<DescriptorRequirement> requirements;
    };

    struct DescriptorSemanticMeta
    {
        DescriptorSetType set_type = DescriptorSetType::Unknow;
        DescriptorKind default_kind = DescriptorKind::UBO;
        const char *name = nullptr;
        const char *binding_macro_name = nullptr;
        const char *struct_name = nullptr;
        const char *glsl_type = nullptr;
    };

    constexpr const char *DescriptorSemanticNameList[] =
    {
        "Unknown",
        "ViewportInfo",
        "CameraInfo",
        "SkyInfo",

        "TransformID",
        "LocalToWorld",

        "MaterialInstanceID",
        "MaterialInstance",

        "MaterialInstanceTextureID",

        "ColorPattle",

        "BoneJoint",
        "BoneJointWeight",

        "Custom"
    };

    constexpr size_t DescriptorSemanticCount = size_t(DescriptorSemantic::Custom) + 1;

    constexpr DescriptorSemanticMeta DescriptorSemanticMetaList[] =
    {
        {DescriptorSetType::Unknow, DescriptorKind::UBO,    nullptr,        nullptr,                nullptr                     }, // Unknown
        {SET_TYPE_VIEWPORT,         DescriptorKind::UBO,    "viewport",     "VIEWPORT_BINDING",     "ViewportInfo"              }, // ViewportInfo
        {SET_TYPE_CAMERA,           DescriptorKind::UBO,    "camera",       "CAMERA_BINDING",       "CameraInfo"                }, // CameraInfo
        {SET_TYPE_SKY,              DescriptorKind::UBO,    "sky",          "SKY_BINDING",          "SkyInfo"                   }, // SkyInfo
        {SET_TYPE_TRANSFORM,        DescriptorKind::SSBO,   "tid",          "TID_BINDING",          "TransformIDData"           }, // TransformID
        {SET_TYPE_TRANSFORM,        DescriptorKind::SSBO,   "l2w",          "L2W_BINDING",          "LocalToWorldData"          }, // LocalToWorld
        {SET_TYPE_MATERIAL,         DescriptorKind::SSBO,   "mid",          "MID_BINDING",          "MaterialInstanceIDData"    }, // MaterialInstanceID
        {SET_TYPE_MATERIAL,         DescriptorKind::SSBO,   "mtl",          "MI_BINDING",           "MaterialInstanceData"      }, // MaterialInstance
        {SET_TYPE_MATERIAL,         DescriptorKind::SSBO,   "mit",          "MIT_BINDING",          "MaterialInstanceTextureID" }, // MaterialInstanceTextureID, 这里存的是每个实例对应的纹理ID（layer index），配合TextureArray使用。所以它是SSBO不是TextureSampler
        {SET_TYPE_MATERIAL,         DescriptorKind::UBO,    "color_pattle", "COLOR_PATTLE_BINDING", "ColorPattle"               }, // ColorPattle
        {SET_TYPE_TRANSFORM,        DescriptorKind::SSBO,   "joint",        "JOINT_BINDING",        "JointInfo"                 }, // BoneJoint
        {SET_TYPE_TRANSFORM,        DescriptorKind::SSBO,   "joint_weight", "JOINT_WEIGHT_BINDING", "JointWeightInfo"           }, // BoneJointWeight
        {DescriptorSetType::Unknow, DescriptorKind::UBO,    nullptr,        nullptr,                nullptr                     }, // Custom
    };

    static_assert(sizeof(DescriptorSemanticNameList) / sizeof(DescriptorSemanticNameList[0]) == DescriptorSemanticCount,
                  "DescriptorSemanticNameList must match DescriptorSemantic enum");
    static_assert(sizeof(DescriptorSemanticMetaList) / sizeof(DescriptorSemanticMetaList[0]) == DescriptorSemanticCount,
                  "DescriptorSemanticMetaList must match DescriptorSemantic enum");

    constexpr const DescriptorSemanticMeta &GetDescriptorSemanticMeta(DescriptorSemantic semantic)
    {
        const size_t index = size_t(semantic);
        if (index < DescriptorSemanticCount)
            return DescriptorSemanticMetaList[index];

        return DescriptorSemanticMetaList[0];
    }

    inline bool _DBC_CStrEq(const char *lhs, const char *rhs)
    {
        return lhs && rhs && std::strcmp(lhs, rhs) == 0;
    }

    inline const char *FindDescriptorBindingMacroNameByDescriptorName(const char *descriptor_name)
    {
        if (!descriptor_name || !*descriptor_name)
            return nullptr;

        for (size_t i = 0; i < DescriptorSemanticCount; ++i)
        {
            const auto &meta = DescriptorSemanticMetaList[i];
            if (_DBC_CStrEq(meta.name, descriptor_name) && meta.binding_macro_name && *meta.binding_macro_name)
                return meta.binding_macro_name;
        }

        return nullptr;
    }

    constexpr FixedDescriptorEntry MakeFixedDescriptorEntry(DescriptorSemantic semantic,
                                                            const uint32_t stage_flags)
    {
        const auto &meta = GetDescriptorSemanticMeta(semantic);
        return FixedDescriptorEntry{
            meta.set_type,
            meta.default_kind,
            stage_flags,
            meta.name,
            meta.struct_name,
            meta.glsl_type,
            semantic
        };
    }

    constexpr FixedDescriptorEntry MakeFixedDescriptorEntry(DescriptorSemantic semantic,
                                                            const uint32_t stage_flags,
                                                            const DescriptorKind kind,
                                                            const char *glsl_type = nullptr)
    {
        const auto &meta = GetDescriptorSemanticMeta(semantic);
        return FixedDescriptorEntry{
            meta.set_type,
            kind,
            stage_flags,
            meta.name,
            meta.struct_name,
            glsl_type ? glsl_type : meta.glsl_type,
            semantic
        };
    }

    inline FixedDescriptorEntry MakeTextureDescriptorEntry(const SamplerSlot slot,
                                                              const uint32_t stage_flags,
                                                              const TextureSourceMode texture_mode = TextureSourceMode::Simple)
    {
        const auto &meta = GetDescriptorSemanticMeta(DescriptorSemantic::MaterialInstanceTextureID);
        return FixedDescriptorEntry{
            meta.set_type,
            DescriptorKind::TextureSampler,  // texture slots are combined image+samplers, not SSBOs
            stage_flags,
            ToDescriptorName(slot),
            meta.struct_name,
            ToGLSLSamplerType(texture_mode),
            DescriptorSemantic::MaterialInstanceTextureID
        };
    }

    inline ResolvedDescriptorRequirement ResolveDescriptorRequirement(const DescriptorRequirement &req)
    {
        const auto &meta = GetDescriptorSemanticMeta(req.semantic);

        ResolvedDescriptorRequirement resolved;
        resolved.semantic = req.semantic;
        resolved.set_type = meta.set_type;
        resolved.kind = req.kind;
        resolved.stage_flags = req.stage_flags;
        resolved.name = req.name_override ? req.name_override : meta.name;
        resolved.struct_name = meta.struct_name;
        resolved.glsl_type = meta.glsl_type;

        return resolved;
    }

    inline DescriptorSetType GetExpectedSetType(DescriptorSemantic semantic)
    {
        return GetDescriptorSemanticMeta(semantic).set_type;
    }

    inline BindingContract BuildBindingContract(const FixedDescriptorEntry *descriptor_entries,
                                                const uint32_t descriptor_entry_count)
    {
        BindingContract contract;
        if (!descriptor_entries || descriptor_entry_count == 0)
            return contract;

        contract.requirements.reserve(descriptor_entry_count);

        for (uint32_t i = 0; i < descriptor_entry_count; ++i)
        {
            const FixedDescriptorEntry &entry = descriptor_entries[i];

            DescriptorRequirement req;
            req.semantic = entry.semantic;

            req.kind = entry.kind;
            req.stage_flags = entry.stage_flags;

            const auto &meta = GetDescriptorSemanticMeta(req.semantic);
            if (entry.name && (!_DBC_CStrEq(entry.name, meta.name) || req.semantic == DescriptorSemantic::Custom))
                req.name_override = entry.name;

            contract.requirements.push_back(req);
        }

        return contract;
    }

    inline const char *GetDescriptorSemanticName(DescriptorSemantic semantic)
    {
        const size_t index = size_t(semantic);
        const size_t count = sizeof(DescriptorSemanticNameList) / sizeof(DescriptorSemanticNameList[0]);

        if (index < count)
            return DescriptorSemanticNameList[index];

        return DescriptorSemanticNameList[0];
    }

    inline bool ValidateBindingContract(const BindingContract &contract, std::vector<std::string> &diagnostics)
    {
        diagnostics.clear();

        for (size_t i = 0; i < contract.requirements.size(); ++i)
        {
            const DescriptorRequirement &req = contract.requirements[i];

            if (req.semantic == DescriptorSemantic::Unknown)
            {
                std::string message = "Descriptor requirement #" + std::to_string(i)
                                      + " has semantic Unknown; string inference is removed, set explicit semantic or use Custom.";

                if (req.name_override && *req.name_override)
                    message += " Name=" + std::string(req.name_override) + ".";

                diagnostics.emplace_back(std::move(message));
                continue;
            }

            if (req.semantic == DescriptorSemantic::Custom)
                continue;
        }

        return diagnostics.empty();
    }
}
