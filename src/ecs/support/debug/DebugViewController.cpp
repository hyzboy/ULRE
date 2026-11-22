#include<hgl/ecs/support/debug/DebugViewController.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/support/material/MaterialAuthoringSideTable.h>
#include<hgl/sync/Mutex.h>
#include<hgl/type/UnorderedMap.h>
#include<hgl/log/Log.h>
#include<cstdlib>

namespace hgl::ecs
{
    namespace
    {
        using namespace hgl::graph::mtl;

        // 接管前的授权事实。
        // `had_override == false` 表示作者原本没有覆盖，还原时必须走
        // ClearRecipeOverride()（SetRecipe({}) 会留下一个空的"真覆盖"）。
        struct SavedRecipe
        {
            bool had_override = false;
            MaterialRecipe recipe{};
            bool overlay_applied = false;
            uint64_t overlay_generation = 0;
        };

        struct ControllerState
        {
            hgl::Mutex lock;

            DebugViewMode requested = DebugViewMode::Off;
            DebugViewMode applied   = DebugViewMode::Off;
            uint32 tracked_count = 0;

            // 被接管的实体 -> 接管前的授权事实（引擎侧旁表，键 = EntityID）。
            hgl::UnorderedMap<EntityID,SavedRecipe> saved;

            // 记录属于哪个世界：换世界时旧记录必须作废（实体 ID 空间不同）。
            ECSContext *world = nullptr;
        };

        ControllerState &State()
        {
            static ControllerState state;

            static const bool env_ready=[]()
            {
                const char *env=getenv("ULRE_DEBUG_VIEW");
                if(!env||!env[0])
                    return true;

                DebugViewMode mode;
                if(!ParseDebugViewMode(env,mode))
                {
                    GLogWarning(u8"[DebugView] ULRE_DEBUG_VIEW invalid: %s",env);
                    return true;
                }

                state.requested=mode;
                return true;
            }();

            (void)env_ready;
            return state;
        }

        void Restore(ECSContext *world,EntityID id,const SavedRecipe &saved)
        {
            MaterialData *material=world?world->GetMaterialData(id):nullptr;
            if(!material || !saved.overlay_applied
               || material->GetRecipeGeneration()!=saved.overlay_generation)
                return;
            if(saved.had_override)
                material->SetRecipe(saved.recipe);
            else
                material->ClearRecipeOverride();
        }

        void DropAllRecords(ECSContext *world)
        {
            ControllerState &state=State();

            // 记录可能属于另一个（已销毁）世界，只在同世界时还原。
            if(world&&state.world==world)
            {
                for(auto [id,saved]:state.saved)
                    Restore(world,id,saved);
            }

            state.saved.Clear();
            state.world=nullptr;
        }

        bool ApplyMode(ECSContext *world,const DebugViewMode mode)
        {
            ControllerState &state=State();
            ValueArray<EntityID> removed;
            for(auto [id,saved]:state.saved)
                if(!world->GetGeometryData(id) || !world->GetMaterialData(id))
                {
                    Restore(world,id,saved);
                    removed.Add(id);
                }
            for(EntityID id:removed)
                state.saved.DeleteByKey(id);

            ValueArray<GeometryData *> primitives;
            world->GetComponents<GeometryData>(primitives);

            uint32 applied=0;

            for(GeometryData *geometry:primitives)
            {
                if(!geometry)
                    continue;

                Entity *owner=geometry->GetOwner();
                if(!owner)
                    continue;

                MaterialData *material=world->GetMaterialData(owner);
                if(!material)
                    continue;   // 无授权记录 = 引擎内部材质，不参与调试视图

                const EntityID id=geometry->GetOwnerID();

                SavedRecipe *saved=state.saved.GetValuePointer(id);

                if(!saved)
                {
                    SavedRecipe fresh;
                    fresh.had_override=material->HasRecipeOverride();

                    if(fresh.had_override)
                        fresh.recipe=*material->GetRecipeOverride();

                    state.saved.Add(id,fresh);
                    saved=state.saved.GetValuePointer(id);
                }

                if(!saved)
                    continue;

                if(saved->overlay_applied
                   && material->GetRecipeGeneration()==saved->overlay_generation)
                    continue;

                saved->had_override=material->HasRecipeOverride();
                saved->recipe=saved->had_override?*material->GetRecipeOverride():MaterialRecipe{};
                saved->overlay_applied=false;

                // base 配方 = 作者原本的配方，不是上一个调试通道的配方：
                // 否则通道之间来回切换会累积（把上一个通道的贴图/渲染状态当成"原始"）。
                const MaterialRecipe *base=saved->had_override?&saved->recipe:nullptr;

                MaterialRecipe debug_recipe;
                if(!BuildDebugViewRecipe(mode,base,debug_recipe))
                    continue;

                material->SetRecipe(debug_recipe);
                saved->overlay_generation=material->GetRecipeGeneration();
                saved->overlay_applied=true;
                ++applied;
            }

            state.world=world;

            if(applied)
                GLogInfo(u8"[DebugView] channel=%s updated=%u/%u",
                         GetDebugViewModeName(mode),applied,
                         static_cast<uint32>(primitives.GetCount()));
            return applied!=0 || !removed.IsEmpty();
        }
    }//namespace

    bool DebugViewController::RequestMode(const DebugViewMode mode)
    {
        if(static_cast<uint32>(mode)>=DebugViewModeCount)
            return false;

        ControllerState &state=State();

        {
            hgl::MutexLock lock(&state.lock);
            state.requested=mode;
        }

        return true;
    }

    DebugViewMode DebugViewController::GetRequestedMode()
    {
        ControllerState &state=State();

        hgl::MutexLock lock(&state.lock);
        return state.requested;
    }

    DebugViewMode DebugViewController::GetAppliedMode()
    {
        ControllerState &state=State();

        hgl::MutexLock lock(&state.lock);
        return state.applied;
    }

    uint32 DebugViewController::GetTrackedCount()
    {
        ControllerState &state=State();

        hgl::MutexLock lock(&state.lock);
        return state.tracked_count;
    }

    bool DebugViewController::Apply(ECSContext *world)
    {
        if(!world)
            return false;

        ControllerState &state=State();

        DebugViewMode want;
        bool refresh=false;

        {
            hgl::MutexLock lock(&state.lock);

            if(state.requested==state.applied)
            {
                if(state.requested==DebugViewMode::Off)
                    return false;
                refresh=state.world==world;
            }

            want=state.requested;
        }

        bool changed=false;
        if(!refresh)
        {
            DropAllRecords(state.world);
            changed=true;
        }
        if(want!=DebugViewMode::Off)
            changed=ApplyMode(world,want)||changed;

        {
            hgl::MutexLock lock(&state.lock);
            state.applied=want;
            state.tracked_count=static_cast<uint32>(state.saved.GetCount());
        }

        return changed;
    }

    void DebugViewController::Reset(ECSContext *world)
    {
        ControllerState &state=State();

        if(world && state.world!=world)
            return;

        DropAllRecords(world?world:state.world);

        hgl::MutexLock lock(&state.lock);
        state.applied=DebugViewMode::Off;
        state.requested=DebugViewMode::Off;
        state.tracked_count=0;
    }
}//namespace hgl::ecs
