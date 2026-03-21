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

        MaterialTexture,        ///<当使用TextureArray时的材质ID信息
        //比如一个材质有BaseColor,Normal两个纹理，但是开发者配置了使用TextureArray
        //那么原本的纹理的Sampler2D TexBaseColor;就会变成Sampler2DArray TexBaseColor;
        //然后在名为MaterialTexture的SSBO里，会出现
        // uint BaseColor[];
        // uint Normal[];
        //这样的数组，用于指定当前材质实例使用哪个纹理Layer，配合TextureArray使用。如有其它类型则自动扩展


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
        bool required = true;
        bool allow_fallback = false;
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

        "MaterialTexture",

        "ColorPattle",

        "BoneJoint",
        "BoneJointWeight",

        "Custom"
    };

    constexpr size_t DescriptorSemanticCount = size_t(DescriptorSemantic::Custom) + 1;

    constexpr DescriptorSemanticMeta DescriptorSemanticMetaList[] =
    {
        { DescriptorSetType::Unknow, DescriptorKind::UBO, nullptr,         nullptr,                 nullptr,                nullptr,      true,  false }, // Unknown
        { SET_TYPE_VIEWPORT,         DescriptorKind::UBO, "viewport",    "VIEWPORT_BINDING",    "ViewportInfo",        nullptr,      true,  false }, // ViewportInfo
        { SET_TYPE_CAMERA,           DescriptorKind::UBO, "camera",      "CAMERA_BINDING",      "CameraInfo",          nullptr,      true,  false }, // CameraInfo
        { SET_TYPE_SKY,              DescriptorKind::UBO, "sky",         "SKY_BINDING",         "SkyInfo",             nullptr,      false, true  }, // SkyInfo
        { SET_TYPE_TRANSFORM,        DescriptorKind::SSBO,"tid",         "TID_BINDING",         "TransformIDData",     nullptr,      true,  false }, // TransformID
        { SET_TYPE_TRANSFORM,        DescriptorKind::SSBO,"l2w",         "L2W_BINDING",         "LocalToWorldData",    nullptr,      true,  false }, // LocalToWorld
        { SET_TYPE_MATERIAL,         DescriptorKind::SSBO,"mid",         "MID_BINDING",         "MaterialInstanceIDData", nullptr,    true,  false }, // MaterialInstanceID
        { SET_TYPE_MATERIAL,         DescriptorKind::SSBO,"mtl",         "MI_BINDING",          "MaterialInstanceData", nullptr,      true,  false }, // MaterialInstance
        { SET_TYPE_TEXTURE,          DescriptorKind::TextureSampler, nullptr, nullptr,              nullptr,                "sampler2D", false, true  }, // MaterialTexture
        { SET_TYPE_MATERIAL,         DescriptorKind::UBO, "color_pattle","COLOR_PATTLE_BINDING","ColorPattle",        nullptr,      true,  false }, // ColorPattle
        { SET_TYPE_TRANSFORM,        DescriptorKind::SSBO,"joint",       "JOINT_BINDING",       "JointInfo",           nullptr,      false, true  }, // BoneJoint
        { SET_TYPE_TRANSFORM,        DescriptorKind::SSBO,"joint_weight","JOINT_WEIGHT_BINDING","JointWeightInfo",     nullptr,      false, true  }, // BoneJointWeight
        { DescriptorSetType::Unknow, DescriptorKind::UBO, nullptr,         nullptr,                 nullptr,                nullptr,      true,  false }, // Custom
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

    inline FixedDescriptorEntry MakeTextureDescriptorEntry(const SamplerName::SamplerSlot slot,
                                                              const uint32_t stage_flags,
                                                              const SamplerName::TextureSourceMode texture_mode = SamplerName::TextureSourceMode::Simple)
    {
        const auto &meta = GetDescriptorSemanticMeta(DescriptorSemantic::MaterialTexture);
        return FixedDescriptorEntry{
            meta.set_type,
            meta.default_kind,
            stage_flags,
            SamplerName::ToDescriptorName(slot),
            meta.struct_name,
            SamplerName::ToGLSLSamplerType(texture_mode),
            DescriptorSemantic::MaterialTexture
        };
    }

    inline DescriptorSemantic InferDescriptorSemantic(const FixedDescriptorEntry &entry)
    {
        if (_DBC_CStrEq(entry.struct_name, "ViewportInfo") || _DBC_CStrEq(entry.name, "viewport"))
            return DescriptorSemantic::ViewportInfo;

        if (_DBC_CStrEq(entry.struct_name, "CameraInfo") || _DBC_CStrEq(entry.name, "camera"))
            return DescriptorSemantic::CameraInfo;

        if (_DBC_CStrEq(entry.struct_name, "SkyInfo") || _DBC_CStrEq(entry.name, "sky"))
            return DescriptorSemantic::SkyInfo;

        if (_DBC_CStrEq(entry.struct_name, "LocalToWorldData") || _DBC_CStrEq(entry.struct_name, "LocalToWorld") || _DBC_CStrEq(entry.name, "l2w"))
            return DescriptorSemantic::LocalToWorld;

        if (_DBC_CStrEq(entry.struct_name, "TransformIDData") || _DBC_CStrEq(entry.struct_name, "TransformID") || _DBC_CStrEq(entry.name, "tid"))
            return DescriptorSemantic::TransformID;

        if (_DBC_CStrEq(entry.struct_name, "MaterialInstanceIDData") || _DBC_CStrEq(entry.struct_name, "MaterialInstanceID") || _DBC_CStrEq(entry.name, "mid"))
            return DescriptorSemantic::MaterialInstanceID;

        if (_DBC_CStrEq(entry.struct_name, "MaterialInstanceData") || _DBC_CStrEq(entry.struct_name, "MaterialInstance") || _DBC_CStrEq(entry.name, "mtl"))
            return DescriptorSemantic::MaterialInstance;

        if (_DBC_CStrEq(entry.struct_name, "ColorPattle") || _DBC_CStrEq(entry.name, "color_pattle"))
            return DescriptorSemantic::ColorPattle;

        if (_DBC_CStrEq(entry.struct_name, "JointInfo") || _DBC_CStrEq(entry.name, "joint"))
            return DescriptorSemantic::BoneJoint;

        if (_DBC_CStrEq(entry.struct_name, "JointWeightInfo") || _DBC_CStrEq(entry.name, "joint_weight"))
            return DescriptorSemantic::BoneJointWeight;

        if (entry.kind == DescriptorKind::Texture || entry.kind == DescriptorKind::TextureSampler)
            return DescriptorSemantic::MaterialTexture;

        if (entry.struct_name || entry.name)
            return DescriptorSemantic::Custom;

        return DescriptorSemantic::Unknown;
    }

    inline bool IsSemanticRequired(DescriptorSemantic semantic)
    {
        return GetDescriptorSemanticMeta(semantic).required;
    }

    inline bool IsSemanticFallbackAllowed(DescriptorSemantic semantic)
    {
        return GetDescriptorSemanticMeta(semantic).allow_fallback;
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
        resolved.required = meta.required;
        resolved.allow_fallback = meta.allow_fallback;

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
            req.semantic = (entry.semantic != DescriptorSemantic::Unknown)
                         ? entry.semantic
                         : InferDescriptorSemantic(entry);
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

        for (const DescriptorRequirement &req : contract.requirements)
        {
            if (req.semantic == DescriptorSemantic::Unknown)
            {
                diagnostics.emplace_back("Descriptor semantic is Unknown; add explicit semantic mapping or mark as Custom.");
                continue;
            }

            if (req.semantic == DescriptorSemantic::Custom)
                continue;
        }

        return diagnostics.empty();
    }
}
