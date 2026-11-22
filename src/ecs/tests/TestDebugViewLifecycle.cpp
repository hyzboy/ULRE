#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/entity/Entity.h>
#include<hgl/ecs/support/debug/DebugViewController.h>
#include<hgl/ecs/support/material/MaterialAuthoringSideTable.h>
#include<hgl/mtl/MaterialDomain.h>
#include<hgl/mtl/MaterialResolver.h>

int main()
{
    using hgl::ecs::DebugViewController;
    using hgl::ecs::ECSContext;
    using hgl::graph::mtl::DebugViewMode;
    using hgl::graph::mtl::MaterialDomain;
    using hgl::graph::mtl::MaterialRecipe;
    using hgl::graph::mtl::ResolvedMaterial;
    using hgl::graph::mtl::ResolveMaterialFromRecipe;
    using hgl::graph::mtl::ResolveMaterialRenderState;
    using hgl::graph::mtl::BuildDebugViewRecipe;

    MaterialRecipe coverage;
    coverage.domain=MaterialDomain::StandardOpaque;
    coverage.render_state_overrides.has_alpha_test=true;
    coverage.render_state_overrides.alpha_test=true;
    coverage.render_state_overrides.has_alpha_cutoff=true;
    coverage.render_state_overrides.alpha_cutoff=0.37f;
    coverage.render_state_overrides.has_dither=true;
    coverage.render_state_overrides.dither=false;
    MaterialRecipe debug;
    if(!BuildDebugViewRecipe(DebugViewMode::ShadingNormal,&coverage,debug))
        return 7;
    ResolvedMaterial resolved{};
    if(!ResolveMaterialFromRecipe(debug,resolved))
        return 8;
    const auto state=ResolveMaterialRenderState(resolved,debug);
    if(!state.alpha_test || state.dither || state.alpha_cutoff!=0.37f
       || !coverage.render_state_overrides.alpha_test)
        return 9;
    coverage.render_state_overrides.dither=true;
    if(BuildDebugViewRecipe(DebugViewMode::ShadingNormal,&coverage,debug))
        return 11;
    coverage.render_state_overrides.dither=false;
    coverage.domain=MaterialDomain::StandardAdvanced;
    if(BuildDebugViewRecipe(DebugViewMode::ShadingNormal,&coverage,debug)
       || BuildDebugViewRecipe(DebugViewMode::ShadingNormal,nullptr,debug))
        return 10;
    coverage.domain=MaterialDomain::StandardOpaque;
    coverage.render_state_overrides.has_pipeline_config=true;
    coverage.render_state_overrides.pipeline_config.alpha_blend=true;
    if(BuildDebugViewRecipe(DebugViewMode::ShadingNormal,&coverage,debug))
        return 12;

    ECSContext world("DebugViewOwner");
    auto *entity=world.CreateEntity<hgl::ecs::Entity>("original");
    if(!entity || !world.GetOrCreateGeometryData(entity)) return 1;
    auto *material=world.GetOrCreateMaterialData(entity);
    if(!material) return 2;
    MaterialRecipe original;
    original.domain=MaterialDomain::StandardOpaque;
    original.recipe_name="original";
    material->SetRecipe(original);
    const uint32_t generation=material->GetAuthoredGeneration();
    if(!DebugViewController::RequestMode(DebugViewMode::WorldNormal)
       || DebugViewController::Apply(&world)
       || DebugViewController::GetRequestedMode()!=DebugViewMode::WorldNormal
       || material->GetAuthoredGeneration()!=generation
       || !material->GetRecipeOverride()
       || material->GetRecipeOverride()->recipe_name!="original")
        return 3;
    if(!DebugViewController::RequestMode(DebugViewMode::WorldPosition)
       || DebugViewController::GetRequestedMode()!=DebugViewMode::WorldPosition
       || material->GetAuthoredGeneration()!=generation)
        return 6;
    {
        ECSContext other("UnrelatedWorld");
        other.Shutdown();
        if(DebugViewController::GetRequestedMode()!=DebugViewMode::WorldPosition)
            return 4;
    }
    if(!DebugViewController::RequestMode(DebugViewMode::Off)
       || DebugViewController::GetRequestedMode()!=DebugViewMode::Off
       || material->GetAuthoredGeneration()!=generation)
        return 5;
    return 0;
}
