#pragma once

#include<hgl/common/DescriptorSemantic.h>
#include<hgl/common/DescriptorSetTypeDef.h>
#include<hgl/common/TextureSamplerTypeDef.h>
#include<hgl/mtl/SamplerName.h>
#include<hgl/vk/BufferPolicy.h>
#include<vector>
#include<map>
#include<string>

namespace hgl::graph::mtl
{
    struct BindingContract
    {
        std::map<UBODescriptorSemantic, uint32_t> ubos;
        std::map<SSBODescriptorSemantic, uint32_t> ssbos;
    };

    struct DescriptorSemanticMeta
    {
        DescriptorSetType set_type = DescriptorSetType::Unknow;
        const char *name = nullptr;
        const char *binding_macro_name = nullptr;
        const char *struct_name = nullptr;
        BufferUpdateClass buffer_update_class = BufferUpdateClass::Default;
    };

    constexpr const char *UBODescriptorSemanticNameList[] =
    {
        "Unknown",
        "ViewportInfo",
        "CameraInfo",
        "SkyInfo",
        "ColorPattle",
        "Custom"
    };

    constexpr const char *SSBODescriptorSemanticNameList[] =
    {
        "Unknown",
        "TransformID",
        "LocalToWorld",
        "MaterialInstanceID",
        "MaterialInstance",
        "MaterialInstanceTextureID",
        "BoneJoint",
        "BoneJointWeight",
        "Custom"
    };

    constexpr size_t UBODescriptorSemanticCount = size_t(UBODescriptorSemantic::Custom) + 1;
    constexpr size_t SSBODescriptorSemanticCount = size_t(SSBODescriptorSemantic::Custom) + 1;

    constexpr bool IsBuiltinDescriptorSemantic(const UBODescriptorSemantic semantic)
    {
        return semantic > UBODescriptorSemantic::Unknown && semantic < UBODescriptorSemantic::Custom;
    }

    constexpr bool IsBuiltinDescriptorSemantic(const SSBODescriptorSemantic semantic)
    {
        return semantic > SSBODescriptorSemantic::Unknown && semantic < SSBODescriptorSemantic::Custom;
    }

    constexpr DescriptorSemanticMeta UBODescriptorSemanticMetaList[] =
    {
        {DescriptorSetType::Unknow, nullptr,        nullptr,                nullptr,        BufferUpdateClass::Default          }, // Unknown
        {SET_TYPE_VIEWPORT,         "viewport",     "VIEWPORT_BINDING",     "ViewportInfo", BufferUpdateClass::CriticalPerFrame }, // ViewportInfo
        {SET_TYPE_CAMERA,           "camera",       "CAMERA_BINDING",       "CameraInfo",   BufferUpdateClass::CriticalPerFrame }, // CameraInfo
        {SET_TYPE_SKY,              "sky",          "SKY_BINDING",          "SkyInfo",      BufferUpdateClass::Deferred         }, // SkyInfo
        {SET_TYPE_MATERIAL,         "color_pattle", "COLOR_PATTLE_BINDING", "ColorPattle",  BufferUpdateClass::Default          }, // ColorPattle
        {DescriptorSetType::Unknow, nullptr,        nullptr,                nullptr,        BufferUpdateClass::Default          }, // Custom
    };

    constexpr DescriptorSemanticMeta SSBODescriptorSemanticMetaList[] =
    {
        {DescriptorSetType::Unknow, nullptr,        nullptr,                nullptr,                     BufferUpdateClass::Default       }, // Unknown
        {SET_TYPE_TRANSFORM,        "tid",          "TID_BINDING",          "TransformIDData",           BufferUpdateClass::TransformData }, // TransformID
        {SET_TYPE_TRANSFORM,        "l2w",          "L2W_BINDING",          "LocalToWorldData",          BufferUpdateClass::TransformData }, // LocalToWorld
        {SET_TYPE_MATERIAL,         "mid",          "MID_BINDING",          "MaterialInstanceIDData",    BufferUpdateClass::Default       }, // MaterialInstanceID
        {SET_TYPE_MATERIAL,         "mtl",          "MI_BINDING",           "MaterialInstanceData",      BufferUpdateClass::Default       }, // MaterialInstance
        {SET_TYPE_MATERIAL,         "mit",          "MIT_BINDING",          "MaterialInstanceTextureID", BufferUpdateClass::Default       }, // MaterialInstanceTextureID, 这里存的是每个实例对应的纹理ID（layer index），配合TextureArray使用。所以它是SSBO不是TextureSampler
        {SET_TYPE_TRANSFORM,        "joint",        "JOINT_BINDING",        "JointInfo",                 BufferUpdateClass::TransformData }, // BoneJoint
        {SET_TYPE_TRANSFORM,        "joint_weight", "JOINT_WEIGHT_BINDING", "JointWeightInfo",           BufferUpdateClass::TransformData }, // BoneJointWeight
        {DescriptorSetType::Unknow, nullptr,        nullptr,                nullptr,                     BufferUpdateClass::Default       }, // Custom
    };

    static_assert(sizeof(UBODescriptorSemanticNameList) / sizeof(UBODescriptorSemanticNameList[0]) == UBODescriptorSemanticCount,
                  "UBODescriptorSemanticNameList must match UBODescriptorSemantic enum");
    static_assert(sizeof(SSBODescriptorSemanticNameList) / sizeof(SSBODescriptorSemanticNameList[0]) == SSBODescriptorSemanticCount,
                  "SSBODescriptorSemanticNameList must match SSBODescriptorSemantic enum");
    static_assert(sizeof(UBODescriptorSemanticMetaList) / sizeof(UBODescriptorSemanticMetaList[0]) == UBODescriptorSemanticCount,
                  "UBODescriptorSemanticMetaList must match UBODescriptorSemantic enum");
    static_assert(sizeof(SSBODescriptorSemanticMetaList) / sizeof(SSBODescriptorSemanticMetaList[0]) == SSBODescriptorSemanticCount,
                  "SSBODescriptorSemanticMetaList must match SSBODescriptorSemantic enum");

    constexpr const DescriptorSemanticMeta &GetDescriptorSemanticMeta(const UBODescriptorSemantic semantic)
    {
        const size_t index = size_t(semantic);
        if (index < UBODescriptorSemanticCount)
            return UBODescriptorSemanticMetaList[index];

        return UBODescriptorSemanticMetaList[0];
    }

    constexpr const DescriptorSemanticMeta &GetDescriptorSemanticMeta(const SSBODescriptorSemantic semantic)
    {
        const size_t index = size_t(semantic);
        if (index < SSBODescriptorSemanticCount)
            return SSBODescriptorSemanticMetaList[index];

        return SSBODescriptorSemanticMetaList[0];
    }

    inline DescriptorSetType GetExpectedSetType(UBODescriptorSemantic semantic)
    {
        return GetDescriptorSemanticMeta(semantic).set_type;
    }

    inline DescriptorSetType GetExpectedSetType(SSBODescriptorSemantic semantic)
    {
        return GetDescriptorSemanticMeta(semantic).set_type;
    }

    inline const char *GetDescriptorSemanticName(UBODescriptorSemantic semantic)
    {
        const size_t index = size_t(semantic);
        if (index < UBODescriptorSemanticCount)
            return UBODescriptorSemanticNameList[index];

        return UBODescriptorSemanticNameList[0];
    }

    inline const char *GetUBODescriptorSemanticName(UBODescriptorSemantic semantic)
    {
        return GetDescriptorSemanticName(semantic);
    }

    inline const char *GetDescriptorSemanticName(SSBODescriptorSemantic semantic)
    {
        const size_t index = size_t(semantic);
        if (index < SSBODescriptorSemanticCount)
            return SSBODescriptorSemanticNameList[index];

        return SSBODescriptorSemanticNameList[0];
    }

    inline const char *GetSSBODescriptorSemanticName(SSBODescriptorSemantic semantic)
    {
        return GetDescriptorSemanticName(semantic);
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
