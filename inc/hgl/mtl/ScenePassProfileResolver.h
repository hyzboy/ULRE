#pragma once

#include <hgl/mtl/PassProfile.h>

namespace hgl::graph::mtl
{
    // Render-preparation output. Provider roots and include paths are always
    // explicit; this resolver never invents identity or fallback modules.
    struct ScenePassProfile
    {
        ShaderModuleSlotRole roles[MaxPassProfileModuleRoots]{};
        AnsiString module_names[MaxPassProfileModuleRoots];
        AnsiString include_paths[MaxPassProfileModuleRoots];
        uint32 module_count = 0;

        bool AddModule(
            ShaderModuleSlotRole role,
            const AnsiString &module_name,
            const AnsiString &include_path = {}) noexcept;
    };

    /// 天光 provider 槽的**规范兜底**模块名：`ambient_light_none`（Null Object，
    /// `EvalIndirectLighting()` 恒返回 `vec3(0.0)`）。
    ///
    /// 任何需要光照的材质都必须有一个 ambient provider —— 宁可喂一个恒零算法，
    /// 也不要让槽空着、由模板去"猜"某个算法。这样 `lighting_model_*` 的间接光
    /// 累加式永远不需要 `#if`，"有没有天光"就只是"喂哪个模块"的一行差异。
    inline constexpr const char *kAmbientLightProviderNone = "ambient_light_none";

    ScenePassProfile MakeIdentityForwardLitProfile() noexcept;
    ScenePassProfile MakeForwardUnlitProfile() noexcept;
    ScenePassProfile MakeSkyProfile() noexcept;
    ScenePassProfile MakeShadowCasterProfile() noexcept;

    // Builds a request from an identity explicitly selected by the caller.
    // The template definition supplies its version; this utility only copies
    // the provided scene roots and never selects a rendering policy.
    bool ResolveScenePassProfileRequest(
        PassProfileID profile_id,
        ShaderStage stage,
        const ScenePassProfile &profile,
        PassProfileRequest &out_request,
        PassProfileValidationDiagnostic &out_diagnostic) noexcept;

}
