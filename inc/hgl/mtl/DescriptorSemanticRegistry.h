#pragma once

#include<hgl/common/DescriptorSemantic.h>
#include<hgl/common/DescriptorSetTypeDef.h>
#include<hgl/common/TextureSamplerTypeDef.h>
#include<hgl/mtl/SamplerSlot.h>
#include<hgl/vk/BufferPolicy.h>
#include<vector>
#include<string>

namespace hgl::graph::mtl
{
    struct DescriptorBindingSlots
    {
        uint32_t ubos[size_t(UBODescriptorSemantic::RANGE_SIZE)] = {};
        uint32_t ssbos[size_t(SSBODescriptorSemantic::RANGE_SIZE)] = {};
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
        "ColorPattle"
    };

    constexpr const char *SSBODescriptorSemanticNameList[] =
    {
        "Unknown",
        "TransformID",
        "TransformData",
        "MaterialInstanceID",
        "MaterialInstanceData",
        "MaterialInstanceTextureID",
        "BoneJoint",
        "BoneJointWeight"
    };

    constexpr size_t UBODescriptorSemanticCount = size_t(UBODescriptorSemantic::RANGE_SIZE);
    constexpr size_t SSBODescriptorSemanticCount = size_t(SSBODescriptorSemantic::RANGE_SIZE);

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
        {SET_TYPE_TRANSFORM,        "tid",          "TID_BINDING",          "TransformID",               BufferUpdateClass::TransformData }, // TransformID
        {SET_TYPE_TRANSFORM,        "l2w",          "L2W_BINDING",          "TransformData",             BufferUpdateClass::TransformData }, // TransformData
        {SET_TYPE_MATERIAL,         "mid",          "MID_BINDING",          "MaterialInstanceID",        BufferUpdateClass::Default       }, // MaterialInstanceID
        {SET_TYPE_MATERIAL,         "mtl",          "MI_BINDING",           "MaterialInstanceData",      BufferUpdateClass::Default       }, // MaterialInstanceData
        {SET_TYPE_MATERIAL,         "mit",          "MIT_BINDING",          "MaterialInstanceTextureID", BufferUpdateClass::Default       }, // MaterialInstanceTextureID, 这里存的是每个实例对应的纹理ID（layer index），配合TextureArray使用。所以它是SSBO不是TextureSampler
        {SET_TYPE_TRANSFORM,        "joint",        "JOINT_BINDING",        "JointInfo",                 BufferUpdateClass::TransformData }, // BoneJoint
        {SET_TYPE_TRANSFORM,        "joint_weight", "JOINT_WEIGHT_BINDING", "JointWeightInfo",           BufferUpdateClass::TransformData }, // BoneJointWeight
        {DescriptorSetType::Unknow, nullptr,        nullptr,                nullptr,                     BufferUpdateClass::Default       }, // Custom
    };

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

    inline bool ValidateBindingContract(const DescriptorBindingSlots &contract, std::vector<std::string> &diagnostics)
    {
        diagnostics.clear();

        for (size_t i = 1; i < UBODescriptorSemanticCount; ++i)
        {
            if (contract.ubos[i] == 0)
                continue;
            if (UBODescriptorSemantic(i) == UBODescriptorSemantic::Unknown)
                diagnostics.emplace_back("UBO slot 0 (Unknown) has non-zero stage_flags; set explicit UBODescriptorSemantic.");
        }

        for (size_t i = 1; i < SSBODescriptorSemanticCount; ++i)
        {
            if (contract.ssbos[i] == 0)
                continue;
            if (SSBODescriptorSemantic(i) == SSBODescriptorSemantic::Unknown)
                diagnostics.emplace_back("SSBO slot 0 (Unknown) has non-zero stage_flags; set explicit SSBODescriptorSemantic.");
        }

        return diagnostics.empty();
    }
}
