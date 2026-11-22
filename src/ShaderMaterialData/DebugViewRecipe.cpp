#include<hgl/mtl/DebugViewMode.h>
#include<hgl/mtl/MaterialDomain.h>
#include<hgl/mtl/MaterialResolver.h>
#include<hgl/log/Log.h>
#include<cstring>

namespace hgl::graph::mtl
{
    const char *GetDebugViewModeName(const DebugViewMode mode) noexcept
    {
        switch(mode)
        {
            case DebugViewMode::Off:         return "off";
            case DebugViewMode::WorldNormal: return "world_normal";
            case DebugViewMode::BaseColor:   return "base_color";
            case DebugViewMode::Surface:     return "surface";
            case DebugViewMode::Emissive:    return "emissive";
            case DebugViewMode::UV0:         return "uv0";
            case DebugViewMode::WorldPosition:return "world_position";
            default:                         return "unknown";
        }
    }

    bool ParseDebugViewMode(const char *name,DebugViewMode &out_mode) noexcept
    {
        if(!name||!name[0])
            return false;

        for(uint32 i=0;i<DebugViewModeCount;i++)
        {
            const DebugViewMode mode=static_cast<DebugViewMode>(i);

            if(hgl::strcmp(GetDebugViewModeName(mode),name)==0)
            {
                out_mode=mode;
                return true;
            }
        }

        return false;
    }

    const char *GetDebugViewCompileDefine(const DebugViewMode mode) noexcept
    {
        switch(mode)
        {
            case DebugViewMode::WorldNormal: return "HGL_DEBUG_VIEW_WORLD_NORMAL";
            case DebugViewMode::BaseColor:   return "HGL_DEBUG_VIEW_BASE_COLOR";
            case DebugViewMode::Surface:     return "HGL_DEBUG_VIEW_SURFACE";
            case DebugViewMode::Emissive:    return "HGL_DEBUG_VIEW_EMISSIVE";
            case DebugViewMode::UV0:         return "HGL_DEBUG_VIEW_UV0";
            case DebugViewMode::WorldPosition:return "HGL_DEBUG_VIEW_WORLD_POSITION";
            default:                         return nullptr;
        }
    }

    bool BuildDebugViewRecipe(const DebugViewMode mode,
                              const MaterialRecipe *base_recipe,
                              MaterialRecipe &out_recipe)
    {
        const char *define_name=GetDebugViewCompileDefine(mode);
        if(!define_name)
            return false;

        if(!GetMaterialDomainContract(MaterialDomain::DebugAttribute))
        {
            GLogError(u8"[DebugView] DebugAttribute 领域无契约，调试通道不可用（mode=%s）",
                      GetDebugViewModeName(mode));
            return false;
        }

        // 从零合成，绝不继承 base 的残留字段（同 BuildFallbackMaterialRecipe 的理由：
        // 残留会让调试配方带上半份旧状态，例如旧通道的 define）。
        out_recipe=MaterialRecipe{};
        out_recipe.domain=MaterialDomain::DebugAttribute;
        out_recipe.recipe_name="ULRE.DebugView.";
        out_recipe.recipe_name+=GetDebugViewModeName(mode);

        if(base_recipe)
        {
            // 材质行**原样保留**：调试通道读的就是这份数据（PBRSurface 行与
            // StandardOpaque 逐字节同构，切通道不搬行）。
            out_recipe.material_ssbo_binding=base_recipe->material_ssbo_binding;

            // 贴图绑定按目标领域槽表过滤后保留：
            //   · 保留 ⇒ "纹理没绑上"在 base_color 通道里表现为纯白（bindless
            //     兜底 1.0），与健康账本的 fallback 记录可以对上；
            //   · 过滤 ⇒ 别家领域的私有槽（如 sprite）不会被塞进本领域导致校验失败。
            for(const RecipeTextureBinding &binding:base_recipe->textures)
            {
                if(FindMaterialDomainTextureSlot(MaterialDomain::DebugAttribute,
                                                 binding.texture_name.c_str()))
                    out_recipe.textures.push_back(binding);
            }

            // 渲染状态：沿用 base 的**已解析结果**，只强制三件事——
            //   · 不混合：半透明会让相邻像素混色，破坏像素统计可比性；
            //   · 不 alpha test / 不 dither：调试通道要覆盖率，不要裁掉像素。
            ResolvedMaterial base_definition{};
            if(!ResolveMaterialFromRecipe(*base_recipe,base_definition))
            {
                GLogError(u8"[DebugView] base 配方无法解析，退回领域默认渲染状态（recipe=%s）",
                          base_recipe->recipe_name.c_str());
                base_definition=ResolvedMaterial{};
            }

            ResolvedMaterialRenderState state=
                ResolveMaterialRenderState(base_definition,*base_recipe);

            state.alpha_test=false;
            state.dither=false;
            state.pipeline_config.alpha_blend=false;

            out_recipe.render_state_overrides.has_alpha_test=true;
            out_recipe.render_state_overrides.alpha_test=false;
            out_recipe.render_state_overrides.has_alpha_cutoff=true;
            out_recipe.render_state_overrides.alpha_cutoff=state.alpha_cutoff;
            out_recipe.render_state_overrides.has_dither=true;
            out_recipe.render_state_overrides.dither=false;
            out_recipe.render_state_overrides.has_pipeline_config=true;
            out_recipe.render_state_overrides.pipeline_config=state.pipeline_config;
            // 掩码 0 = 整份替换（与 NormalizeRecipe 的回写口径一致）。
            out_recipe.render_state_overrides.pipeline_config_mask=0;
        }

        // 通道选择：**整表替换** base 的 defines，避免把材质自己的宏（如
        // TEXT_SDF_ENABLED）带进调试通道 —— 那会让通道内容随材质变化。
        out_recipe.has_compile_defines=true;
        out_recipe.compile_defines.clear();
        out_recipe.compile_defines.emplace_back(define_name);

        // 与普通配方同一条规范化路径（定义默认值 + 解析后的渲染状态）。
        NormalizeRecipe(out_recipe);
        return true;
    }
}//namespace hgl::graph::mtl
