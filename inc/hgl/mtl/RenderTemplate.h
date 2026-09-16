#pragma once

#include <hgl/common/ShaderStageDef.h>
#include <hgl/type/String.h>
#include <hgl/util/hash/FNV1a.h>

namespace hgl::graph::mtl
{
    class ShaderCodeModuleRegistry;
    // 仅作指针参数使用；完整定义见 ShaderCodeResourceManifest.h。
    // 不可在此 include——该头经 ShaderCodeModule.h 反向依赖本文件，会形成环。
    struct ShaderCodeResourceManifest;

    enum class RenderTemplateID : uint8
    {
        Unknown = 0,
        ForwardLitShadowedAO,
        ForwardLitShadowedIdentityAO,
        ForwardLitUnshadowedAO,
        ForwardUnlit,
        ShadowCasterOpaque,
        ShadowCasterMasked,
        Sky
    };

    enum class ShaderModuleSlotRole : uint8
    {
        Unknown = 0,
        SurfaceProvider,
        DirectLightProvider,
        ShadowProvider,
        AmbientLightProvider,
        AmbientOcclusionProvider,
        LightingModel,
        OutputPolicy,
        MaterialSourceProvider,
        NTBProvider
    };

    constexpr uint32 MaxRenderTemplateModuleRoots = 10;

    struct RenderTemplateModuleRoot
    {
        ShaderModuleSlotRole role = ShaderModuleSlotRole::Unknown;
        AnsiString module_name;
        AnsiString include_path;
    };

    struct RenderTemplateRequest
    {
        RenderTemplateID template_id = RenderTemplateID::Unknown;
        ShaderStage stage = ShaderStage::Fragment;
        uint32 template_version = 0;
        RenderTemplateModuleRoot module_roots[
            MaxRenderTemplateModuleRoots]{};
        uint32 module_root_count = 0;

        bool AddModuleRoot(
            ShaderModuleSlotRole role,
            const AnsiString &module_name) noexcept;

        const RenderTemplateModuleRoot *FindModuleRoot(
            ShaderModuleSlotRole role) const noexcept;

        uint64 GetHash() const noexcept;
    };

    struct RenderTemplateSlot
    {
        ShaderModuleSlotRole role = ShaderModuleSlotRole::Unknown;
        bool required = true;
    };

    struct RenderTemplateDefinition
    {
        RenderTemplateID id = RenderTemplateID::Unknown;
        const char *name = nullptr;
        ShaderStage stage = ShaderStage::Fragment;
        uint32 version = 0;

        /// slots 的数组顺序即 fragment 模块的 #include 发射顺序，
        /// FragmentTemplateComposer 按此顺序遍历（见 AppendSlotIncludes）。
        /// 重排会改变生成的 GLSL 文本 → program hash 变化 → SPV 缓存失效。
        ///
        /// 硬约束：SurfaceProvider 必须排在 MaterialSourceProvider 与
        /// NTBProvider 之后——surface/material_surface.glsl 直接调用
        /// EvalMaterialSource / EvalMaterialAlpha / GetNTB，GLSL 要求被调函数
        /// 先于调用点声明。
        ///
        /// 若某模板声明了但并不 include 某个 slot（当前仅 Sky / ShadowCaster
        /// 的 OutputPolicy：WriteMaterialOutput 由模板内联生成），该 slot 排在
        /// 数组末尾并在定义处注明。
        const RenderTemplateSlot *slots = nullptr;
        uint32 slot_count = 0;
    };

    enum class RenderTemplateValidationError : uint8
    {
        None = 0,
        UnknownTemplate,
        StageMismatch,
        VersionMismatch,
        EmptyModuleRoot,
        UnknownSlotRole,
        UnexpectedSlotRole,
        DuplicateSlotRole,
        MissingRequiredSlot
        ,ModuleNotFound
        ,ModuleSlotMismatch
        ,MissingModuleCapability
        ,ModuleGraphInvalid
        ,ModuleConflict
    };

    struct RenderTemplateValidationDiagnostic
    {
        RenderTemplateValidationError error =
            RenderTemplateValidationError::None;
        RenderTemplateID template_id = RenderTemplateID::Unknown;
        ShaderModuleSlotRole role = ShaderModuleSlotRole::Unknown;
        AnsiString module_name;
    };

    const char *GetRenderTemplateName(RenderTemplateID id) noexcept;
    const char *GetShaderModuleSlotRoleName(ShaderModuleSlotRole role) noexcept;
    const char *GetRenderTemplateValidationErrorName(
        RenderTemplateValidationError error) noexcept;

    const RenderTemplateDefinition *FindRenderTemplate(
        RenderTemplateID id) noexcept;

    bool ValidateRenderTemplateRequest(
        const RenderTemplateRequest &request,
        RenderTemplateValidationDiagnostic &out_diagnostic) noexcept;

    /// 校验渲染模板请求（含模块图与能力闭合）。
    ///
    /// out_manifest 可选：校验过程必须构建整张模块依赖图，调用方（如
    /// ResolveRenderTemplate）若同样需要该 manifest，可传入以避免重复构建。
    bool ValidateRenderTemplateRequest(
        const RenderTemplateRequest &request,
        const ShaderCodeModuleRegistry &module_registry,
        RenderTemplateValidationDiagnostic &out_diagnostic,
        ShaderCodeResourceManifest *out_manifest = nullptr) noexcept;

}
