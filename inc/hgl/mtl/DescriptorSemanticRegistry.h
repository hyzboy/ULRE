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
        "ColorPalette"
    };

    constexpr const char *SSBODescriptorSemanticNameList[] =
    {
        "Unknown",
        "TransformID",
        "TransformData",
        "MaterialBindingInstanceID",
        "MaterialBindingInstanceData",
        "MaterialBindingInstanceTexture",
        "BoneJoint",
        "BoneJointWeight",
        "VertexStreams",
        "IndexStreams"
    };

    constexpr size_t UBODescriptorSemanticCount = size_t(UBODescriptorSemantic::RANGE_SIZE);
    constexpr size_t SSBODescriptorSemanticCount = size_t(SSBODescriptorSemantic::RANGE_SIZE);

        constexpr DescriptorSemanticMeta UBODescriptorSemanticMetaList[] =
    {
        {DescriptorSetType::Unknow, nullptr,        nullptr,                nullptr,        BufferUpdateClass::Default          }, // Unknown
        {SET_TYPE_VIEWPORT,         "viewport",     "VIEWPORT_BINDING",     "ViewportInfo", BufferUpdateClass::CriticalPerFrame }, // ViewportInfo
        {SET_TYPE_CAMERA,           "camera",       "CAMERA_BINDING",       "CameraInfo",   BufferUpdateClass::CriticalPerFrame }, // CameraInfo
        {SET_TYPE_SKY,              "sky",          "SKY_BINDING",          "SkyInfo",      BufferUpdateClass::Deferred         }, // SkyInfo
        {SET_TYPE_MATERIAL,         "color_palette", "COLOR_PALETTE_BINDING", "ColorPalette",  BufferUpdateClass::Default          }, // ColorPalette
        {DescriptorSetType::Unknow, nullptr,        nullptr,                nullptr,        BufferUpdateClass::Default          }, // Custom
    };

    constexpr DescriptorSemanticMeta SSBODescriptorSemanticMetaList[] =
    {
        {DescriptorSetType::Unknow, nullptr,            nullptr,                    nullptr,                            BufferUpdateClass::Default       }, // Unknown
        {SET_TYPE_TRANSFORM,        "transform_id",     "TRANSFORM_ID_BINDING",     "TransformID",                      BufferUpdateClass::TransformData }, // TransformID
        {SET_TYPE_TRANSFORM,        "transform_data",   "TRANSFORM_DATA_BINDING",   "TransformData",                    BufferUpdateClass::TransformData }, // TransformData
        {SET_TYPE_MATERIAL,         "mbi_id",           "MBI_ID_BINDING",           "MaterialBindingInstanceID",        BufferUpdateClass::Default       }, // MaterialBindingInstanceID
        {SET_TYPE_MATERIAL,         "mbi_data",         "MBI_DATA_BINDING",         "MaterialBindingInstanceData",      BufferUpdateClass::Default       }, // MaterialBindingInstanceData
        {SET_TYPE_MATERIAL,         "mbi_texture",      "MBI_TEXTURE_BINDING",      "MaterialBindingInstanceTexture",   BufferUpdateClass::Default       }, // MaterialBindingInstanceTexture, 这里存的是每个实例对应的纹理ID（layer index），配合TextureArray使用。所以它是SSBO不是TextureSampler
        {SET_TYPE_TRANSFORM,        "joint",            "JOINT_BINDING",            "Joint",                            BufferUpdateClass::TransformData }, // BoneJoint
        {SET_TYPE_TRANSFORM,        "joint_weight",     "JOINT_WEIGHT_BINDING",     "JointWeight",                      BufferUpdateClass::TransformData }, // BoneJointWeight
        {SET_TYPE_VERTEX_STREAMS,   "vertex_streams",   nullptr,                     nullptr,                             BufferUpdateClass::Default       }, // VertexStreams: set ownership marker; binding is explicit VertexAttrib ordinal
        {SET_TYPE_VERTEX_STREAMS,   "index_streams",    nullptr,                     nullptr,                             BufferUpdateClass::Default       }, // IndexStreams: set ownership marker for index-stream SSBOs in VertexStreams set
        {DescriptorSetType::Unknow, nullptr,            nullptr,                    nullptr,                            BufferUpdateClass::Default       }, // Custom
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
