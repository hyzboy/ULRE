#pragma once

#include <hgl/CoreType.h>
#include <hgl/mtl/DescriptorSemantic.h>
#include <hgl/graph/ssbo/SSBOTypes.h>
#include <hgl/graph/ssbo/TextureSlot.h>

namespace hgl::graph::mtl
{
    // Reusable GLSL code is stage-agnostic. A module may be used by vertex,
    // fragment, or shared shader generation paths.
    enum class GLSLCodeModuleID : uint16
    {
        SkyLightHeader = 0,
        SkyLightSimple,
        SkyLightCubeMap,
        ENUM_CLASS_RANGE(SkyLightHeader, SkyLightCubeMap)
    };

    struct GLSLCodeModuleUBORequirement
    {
        UBODescriptorSemantic semantic = UBODescriptorSemantic::ViewportInfo;
        uint32 stage_flags = 0;
    };

    struct GLSLCodeModuleSSBORequirement
    {
        const char *name = nullptr;
        SSBOType ssbo_type = SSBOType::UserDefined;
        uint32 ssbo_slot = 0;
        uint32 stage_flags = 0;
    };

    struct GLSLCodeModuleTextureRequirement
    {
        const char *name = nullptr;
        const char *glsl_type = nullptr;
        DescriptorSemantic semantic = DescriptorSemantic::MaterialTexture;
        TextureSlot slot = TextureSlot::BaseColor;
        uint32 stage_flags = 0;
        bool required = true;
    };

    struct GLSLCodeModuleDefinition
    {
        GLSLCodeModuleID id = GLSLCodeModuleID::SkyLightHeader;
        const char *name = nullptr;
        const char *glsl_code = nullptr;

        const GLSLCodeModuleUBORequirement *ubo_requirements = nullptr;
        uint32 ubo_requirement_count = 0;

        const GLSLCodeModuleSSBORequirement *ssbo_requirements = nullptr;
        uint32 ssbo_requirement_count = 0;

        const GLSLCodeModuleTextureRequirement *texture_requirements = nullptr;
        uint32 texture_requirement_count = 0;

        const GLSLCodeModuleID *code_module_requirements = nullptr;
        uint32 code_module_requirement_count = 0;
    };

    const GLSLCodeModuleDefinition *FindGLSLCodeModuleDefinition(GLSLCodeModuleID id) noexcept;
    const char *GetGLSLCodeModuleName(GLSLCodeModuleID id) noexcept;
}
