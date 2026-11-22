#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/entity/Entity.h>
#include<hgl/ecs/support/debug/DebugViewController.h>
#include<hgl/ecs/support/material/MaterialAuthoringSideTable.h>
#include<hgl/mtl/MaterialDomain.h>

int main()
{
    using hgl::ecs::DebugViewController;
    using hgl::ecs::ECSContext;
    using hgl::graph::mtl::DebugViewMode;
    using hgl::graph::mtl::MaterialDomain;
    using hgl::graph::mtl::MaterialRecipe;

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
    {
        ECSContext other("UnrelatedWorld");
        other.Shutdown();
        if(DebugViewController::GetRequestedMode()!=DebugViewMode::WorldNormal)
            return 4;
    }
    if(!DebugViewController::RequestMode(DebugViewMode::Off)
       || DebugViewController::GetRequestedMode()!=DebugViewMode::Off
       || material->GetAuthoredGeneration()!=generation)
        return 5;
    return 0;
}
