#pragma once

#include<hgl/common/DescriptorSemantic.h>
#include<hgl/common/DescriptorSetTypeDef.h>
#include<hgl/common/TextureSamplerTypeDef.h>
#include<hgl/mtl/SamplerName.h>
#include<hgl/vk/BufferPolicy.h>
#include<vector>
#include<map>
#include<string>
#include<cstring>

namespace hgl::graph::mtl
{
    struct FixedDescriptorEntry
    {
        DescriptorSetType   set_type;
        DescriptorKind      kind;
        uint32_t            stage_flags;
        const char *        name;
        const char *        struct_name;
        TextureType         texture_type=TextureType::Error;
        SamplerType         sampler_type=SamplerType::Error;
        DescriptorSemantic  semantic = DescriptorSemantic::Unknown;
    };

    struct BindingContract
    {
        std::map<UBODescriptorSemantic, uint32_t> ubos;
        std::map<SSBODescriptorSemantic, uint32_t> ssbos;
    };

    struct DescriptorSemanticMeta
    {
        DescriptorSetType set_type = DescriptorSetType::Unknow;
        DescriptorKind default_kind = DescriptorKind::UBO;
        const char *name = nullptr;
        const char *binding_macro_name = nullptr;
        const char *struct_name = nullptr;
        const char *glsl_type = nullptr;
        BufferUpdateClass buffer_update_class = BufferUpdateClass::Default;
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
    constexpr size_t UBODescriptorSemanticCount = size_t(UBODescriptorSemantic::Custom) + 1;
    constexpr size_t SSBODescriptorSemanticCount = size_t(SSBODescriptorSemantic::Custom) + 1;

    constexpr bool IsBuiltinDescriptorSemantic(const DescriptorSemantic semantic)
    {
        return semantic > DescriptorSemantic::Unknown && semantic < DescriptorSemantic::Custom;
    }

    constexpr bool IsBuiltinDescriptorSemantic(const UBODescriptorSemantic semantic)
    {
        return semantic > UBODescriptorSemantic::Unknown && semantic < UBODescriptorSemantic::Custom;
    }

    constexpr bool IsBuiltinDescriptorSemantic(const SSBODescriptorSemantic semantic)
    {
        return semantic > SSBODescriptorSemantic::Unknown && semantic < SSBODescriptorSemantic::Custom;
    }

    constexpr DescriptorSemanticMeta DescriptorSemanticMetaList[] =
    {
        {DescriptorSetType::Unknow, DescriptorKind::UBO,    nullptr,        nullptr,                nullptr,                     nullptr, BufferUpdateClass::Default          }, // Unknown
        {SET_TYPE_VIEWPORT,         DescriptorKind::UBO,    "viewport",     "VIEWPORT_BINDING",     "ViewportInfo",              nullptr, BufferUpdateClass::CriticalPerFrame }, // ViewportInfo
        {SET_TYPE_CAMERA,           DescriptorKind::UBO,    "camera",       "CAMERA_BINDING",       "CameraInfo",                nullptr, BufferUpdateClass::CriticalPerFrame }, // CameraInfo
        {SET_TYPE_SKY,              DescriptorKind::UBO,    "sky",          "SKY_BINDING",          "SkyInfo",                   nullptr, BufferUpdateClass::Deferred         }, // SkyInfo
        {SET_TYPE_TRANSFORM,        DescriptorKind::SSBO,   "tid",          "TID_BINDING",          "TransformIDData",           nullptr, BufferUpdateClass::TransformData    }, // TransformID
        {SET_TYPE_TRANSFORM,        DescriptorKind::SSBO,   "l2w",          "L2W_BINDING",          "LocalToWorldData",          nullptr, BufferUpdateClass::TransformData    }, // LocalToWorld
        {SET_TYPE_MATERIAL,         DescriptorKind::SSBO,   "mid",          "MID_BINDING",          "MaterialInstanceIDData",    nullptr, BufferUpdateClass::Default          }, // MaterialInstanceID
        {SET_TYPE_MATERIAL,         DescriptorKind::SSBO,   "mtl",          "MI_BINDING",           "MaterialInstanceData",      nullptr, BufferUpdateClass::Default          }, // MaterialInstance
        {SET_TYPE_MATERIAL,         DescriptorKind::SSBO,   "mit",          "MIT_BINDING",          "MaterialInstanceTextureID", nullptr, BufferUpdateClass::Default          }, // MaterialInstanceTextureID, 这里存的是每个实例对应的纹理ID（layer index），配合TextureArray使用。所以它是SSBO不是TextureSampler
        {SET_TYPE_MATERIAL,         DescriptorKind::UBO,    "color_pattle", "COLOR_PATTLE_BINDING", "ColorPattle",               nullptr, BufferUpdateClass::Default          }, // ColorPattle
        {SET_TYPE_TRANSFORM,        DescriptorKind::SSBO,   "joint",        "JOINT_BINDING",        "JointInfo",                 nullptr, BufferUpdateClass::TransformData    }, // BoneJoint
        {SET_TYPE_TRANSFORM,        DescriptorKind::SSBO,   "joint_weight", "JOINT_WEIGHT_BINDING", "JointWeightInfo",           nullptr, BufferUpdateClass::TransformData    }, // BoneJointWeight
        {DescriptorSetType::Unknow, DescriptorKind::UBO,    nullptr,        nullptr,                nullptr,                     nullptr, BufferUpdateClass::Default          }, // Custom
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

    constexpr DescriptorSemantic ToDescriptorSemantic(const UBODescriptorSemantic semantic)
    {
        switch (semantic)
        {
        case UBODescriptorSemantic::ViewportInfo: return DescriptorSemantic::ViewportInfo;
        case UBODescriptorSemantic::CameraInfo:   return DescriptorSemantic::CameraInfo;
        case UBODescriptorSemantic::SkyInfo:      return DescriptorSemantic::SkyInfo;
        case UBODescriptorSemantic::ColorPattle:  return DescriptorSemantic::ColorPattle;
        case UBODescriptorSemantic::Custom:       return DescriptorSemantic::Custom;
        case UBODescriptorSemantic::Unknown:
        default:                                  return DescriptorSemantic::Unknown;
        }
    }

    constexpr DescriptorSemantic ToDescriptorSemantic(const SSBODescriptorSemantic semantic)
    {
        switch (semantic)
        {
        case SSBODescriptorSemantic::TransformID:               return DescriptorSemantic::TransformID;
        case SSBODescriptorSemantic::LocalToWorld:              return DescriptorSemantic::LocalToWorld;
        case SSBODescriptorSemantic::MaterialInstanceID:        return DescriptorSemantic::MaterialInstanceID;
        case SSBODescriptorSemantic::MaterialInstance:          return DescriptorSemantic::MaterialInstance;
        case SSBODescriptorSemantic::MaterialInstanceTextureID: return DescriptorSemantic::MaterialInstanceTextureID;
        case SSBODescriptorSemantic::BoneJoint:                 return DescriptorSemantic::BoneJoint;
        case SSBODescriptorSemantic::BoneJointWeight:           return DescriptorSemantic::BoneJointWeight;
        case SSBODescriptorSemantic::Custom:                    return DescriptorSemantic::Custom;
        case SSBODescriptorSemantic::Unknown:
        default:                                                return DescriptorSemantic::Unknown;
        }
    }

    constexpr UBODescriptorSemantic ToUBODescriptorSemantic(const DescriptorSemantic semantic)
    {
        switch (semantic)
        {
        case DescriptorSemantic::ViewportInfo: return UBODescriptorSemantic::ViewportInfo;
        case DescriptorSemantic::CameraInfo:   return UBODescriptorSemantic::CameraInfo;
        case DescriptorSemantic::SkyInfo:      return UBODescriptorSemantic::SkyInfo;
        case DescriptorSemantic::ColorPattle:  return UBODescriptorSemantic::ColorPattle;
        case DescriptorSemantic::Custom:       return UBODescriptorSemantic::Custom;
        case DescriptorSemantic::Unknown:
        default:                               return UBODescriptorSemantic::Unknown;
        }
    }

    constexpr SSBODescriptorSemantic ToSSBODescriptorSemantic(const DescriptorSemantic semantic)
    {
        switch (semantic)
        {
        case DescriptorSemantic::TransformID:               return SSBODescriptorSemantic::TransformID;
        case DescriptorSemantic::LocalToWorld:              return SSBODescriptorSemantic::LocalToWorld;
        case DescriptorSemantic::MaterialInstanceID:        return SSBODescriptorSemantic::MaterialInstanceID;
        case DescriptorSemantic::MaterialInstance:          return SSBODescriptorSemantic::MaterialInstance;
        case DescriptorSemantic::MaterialInstanceTextureID: return SSBODescriptorSemantic::MaterialInstanceTextureID;
        case DescriptorSemantic::BoneJoint:                 return SSBODescriptorSemantic::BoneJoint;
        case DescriptorSemantic::BoneJointWeight:           return SSBODescriptorSemantic::BoneJointWeight;
        case DescriptorSemantic::Custom:                    return SSBODescriptorSemantic::Custom;
        case DescriptorSemantic::Unknown:
        default:                                            return SSBODescriptorSemantic::Unknown;
        }
    }

    constexpr const DescriptorSemanticMeta &GetDescriptorSemanticMeta(const UBODescriptorSemantic semantic)
    {
        return GetDescriptorSemanticMeta(ToDescriptorSemantic(semantic));
    }

    constexpr const DescriptorSemanticMeta &GetDescriptorSemanticMeta(const SSBODescriptorSemantic semantic)
    {
        return GetDescriptorSemanticMeta(ToDescriptorSemantic(semantic));
    }

    static_assert(GetDescriptorSemanticMeta(UBODescriptorSemantic::ViewportInfo).default_kind == DescriptorKind::UBO,
                  "UBODescriptorSemantic::ViewportInfo must map to UBO kind");
    static_assert(GetDescriptorSemanticMeta(UBODescriptorSemantic::CameraInfo).default_kind == DescriptorKind::UBO,
                  "UBODescriptorSemantic::CameraInfo must map to UBO kind");
    static_assert(GetDescriptorSemanticMeta(UBODescriptorSemantic::SkyInfo).default_kind == DescriptorKind::UBO,
                  "UBODescriptorSemantic::SkyInfo must map to UBO kind");
    static_assert(GetDescriptorSemanticMeta(UBODescriptorSemantic::ColorPattle).default_kind == DescriptorKind::UBO,
                  "UBODescriptorSemantic::ColorPattle must map to UBO kind");

    static_assert(GetDescriptorSemanticMeta(SSBODescriptorSemantic::TransformID).default_kind == DescriptorKind::SSBO,
                  "SSBODescriptorSemantic::TransformID must map to SSBO kind");
    static_assert(GetDescriptorSemanticMeta(SSBODescriptorSemantic::LocalToWorld).default_kind == DescriptorKind::SSBO,
                  "SSBODescriptorSemantic::LocalToWorld must map to SSBO kind");
    static_assert(GetDescriptorSemanticMeta(SSBODescriptorSemantic::MaterialInstanceID).default_kind == DescriptorKind::SSBO,
                  "SSBODescriptorSemantic::MaterialInstanceID must map to SSBO kind");
    static_assert(GetDescriptorSemanticMeta(SSBODescriptorSemantic::MaterialInstance).default_kind == DescriptorKind::SSBO,
                  "SSBODescriptorSemantic::MaterialInstance must map to SSBO kind");
    static_assert(GetDescriptorSemanticMeta(SSBODescriptorSemantic::MaterialInstanceTextureID).default_kind == DescriptorKind::SSBO,
                  "SSBODescriptorSemantic::MaterialInstanceTextureID must map to SSBO kind");
    static_assert(GetDescriptorSemanticMeta(SSBODescriptorSemantic::BoneJoint).default_kind == DescriptorKind::SSBO,
                  "SSBODescriptorSemantic::BoneJoint must map to SSBO kind");
    static_assert(GetDescriptorSemanticMeta(SSBODescriptorSemantic::BoneJointWeight).default_kind == DescriptorKind::SSBO,
                  "SSBODescriptorSemantic::BoneJointWeight must map to SSBO kind");

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
            TextureType::Error,
            SamplerType::Error,
            semantic
        };
    }

    constexpr FixedDescriptorEntry MakeFixedDescriptorEntry(const UBODescriptorSemantic semantic,
                                                            const uint32_t stage_flags)
    {
        return MakeFixedDescriptorEntry(ToDescriptorSemantic(semantic),stage_flags);
    }

    constexpr FixedDescriptorEntry MakeFixedDescriptorEntry(const SSBODescriptorSemantic semantic,
                                                            const uint32_t stage_flags)
    {
        return MakeFixedDescriptorEntry(ToDescriptorSemantic(semantic),stage_flags);
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
            TextureType::Error,
            (texture_mode==TextureSourceMode::Array?SamplerType::Sampler2DArray:SamplerType::Sampler2D),
            DescriptorSemantic::MaterialInstanceTextureID
        };
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

        for (uint32_t i = 0; i < descriptor_entry_count; ++i)
        {
            const FixedDescriptorEntry &entry = descriptor_entries[i];

            switch (entry.kind)
            {
            case DescriptorKind::UBO:
            {
                const auto semantic = ToUBODescriptorSemantic(entry.semantic);
                if (semantic != UBODescriptorSemantic::Unknown)
                    contract.ubos[semantic] |= entry.stage_flags;
                break;
            }
            case DescriptorKind::SSBO:
            {
                const auto semantic = ToSSBODescriptorSemantic(entry.semantic);
                if (semantic != SSBODescriptorSemantic::Unknown)
                    contract.ssbos[semantic] |= entry.stage_flags;
                break;
            }
            default:
                break;
            }
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

        auto validate_ubo_map = [&diagnostics](const std::map<UBODescriptorSemantic, uint32_t> &requirements)
        {
            size_t i = 0;
            for (const auto &[semantic, stage_flags] : requirements)
            {
                if (semantic == UBODescriptorSemantic::Unknown)
                {
                    std::string message = "UBO requirement #" + std::to_string(i)
                                          + " has semantic Unknown; set explicit UBODescriptorSemantic.";
                    diagnostics.emplace_back(std::move(message));
                }

                (void)stage_flags;
                ++i;
            }
        };

        auto validate_ssbo_map = [&diagnostics](const std::map<SSBODescriptorSemantic, uint32_t> &requirements)
        {
            size_t i = 0;
            for (const auto &[semantic, stage_flags] : requirements)
            {
                if (semantic == SSBODescriptorSemantic::Unknown)
                {
                    std::string message = "SSBO requirement #" + std::to_string(i)
                                          + " has semantic Unknown; set explicit SSBODescriptorSemantic.";
                    diagnostics.emplace_back(std::move(message));
                }

                (void)stage_flags;
                ++i;
            }
        };

        validate_ubo_map(contract.ubos);
        validate_ssbo_map(contract.ssbos);

        return diagnostics.empty();
    }
}
