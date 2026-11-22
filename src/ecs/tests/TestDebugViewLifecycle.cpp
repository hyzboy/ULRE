#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/support/debug/DebugViewController.h>
#include<hgl/ecs/support/material/MaterialAuthoringSideTable.h>
#include<hgl/ecs/core/entity/Entity.h>
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
    if(!entity || !world.GetOrCreateGeometryData(entity)) return 5;
    auto *material=world.GetOrCreateMaterialData(entity);
    if(!material) return 6;
    MaterialRecipe original;
    original.domain=MaterialDomain::StandardOpaque;
    original.recipe_name="original";
    material->SetRecipe(original);
    if(!DebugViewController::RequestMode(DebugViewMode::WorldNormal)
       || !DebugViewController::Apply(&world)
       || DebugViewController::GetAppliedMode()!=DebugViewMode::WorldNormal
       || !material->GetRecipeOverride()
       || material->GetRecipeOverride()->domain!=MaterialDomain::DebugAttribute)
        return 1;
    MaterialRecipe edited=original;
    edited.recipe_name="edited";
    material->SetRecipe(edited);
    if(!DebugViewController::Apply(&world)
       || material->GetRecipeOverride()->domain!=MaterialDomain::DebugAttribute)
        return 7;
    auto *added=world.CreateEntity<hgl::ecs::Entity>("added");
    if(!added || !world.GetOrCreateGeometryData(added)) return 8;
    auto *added_material=world.GetOrCreateMaterialData(added);
    material=world.GetMaterialData(entity);
    if(!added_material || !DebugViewController::Apply(&world)
       || !added_material->GetRecipeOverride()
       || added_material->GetRecipeOverride()->domain!=MaterialDomain::DebugAttribute)
        return 9;
    material->BumpAuthoredGeneration();
    if(DebugViewController::Apply(&world))
        return 14;
    if(!DebugViewController::RequestMode(DebugViewMode::Off)
       || !DebugViewController::Apply(&world)
       || !material->GetRecipeOverride()
       || material->GetRecipeOverride()->recipe_name!="edited"
       || added_material->HasRecipeOverride())
        return 10;
    if(!DebugViewController::RequestMode(DebugViewMode::WorldNormal)
       || !DebugViewController::Apply(&world))
        return 11;
    MaterialRecipe latest=original;
    latest.recipe_name="latest";
    material->SetRecipe(latest);
    if(!DebugViewController::RequestMode(DebugViewMode::Off)
       || !DebugViewController::Apply(&world)
       || !material->GetRecipeOverride()
       || material->GetRecipeOverride()->recipe_name!="latest")
        return 12;
    if(!DebugViewController::RequestMode(DebugViewMode::WorldNormal)
       || !DebugViewController::Apply(&world))
        return 13;
    {
        ECSContext other("UnrelatedWorld");
        other.Shutdown();
        if(DebugViewController::GetAppliedMode()!=DebugViewMode::WorldNormal)
            return 2;
    }
    if(DebugViewController::GetAppliedMode()!=DebugViewMode::WorldNormal)
        return 3;
    world.Shutdown();
    if(DebugViewController::GetAppliedMode()!=DebugViewMode::Off
       || DebugViewController::GetRequestedMode()!=DebugViewMode::Off)
        return 4;
    return 0;
}
