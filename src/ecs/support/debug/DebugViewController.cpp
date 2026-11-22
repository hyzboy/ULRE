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

        // 按记录还原并清空。world 可以是 nullptr（换世界时）。
        void DropAllRecords(ECSContext *world)
        {
            ControllerState &state=State();

            // 记录可能属于另一个（已销毁）世界，只在同世界时还原。
            if(world&&state.world==world)
            {
                for(auto [id,saved]:state.saved)
                {
                    MaterialData *material=world->GetMaterialData(id);
                    if(!material)
                        continue;

                    if(saved.had_override)
                        material->SetRecipe(saved.recipe);
                    else
                        material->ClearRecipeOverride();
                }
            }

            state.saved.Clear();
            state.world=nullptr;
        }

        void ApplyMode(ECSContext *world,const DebugViewMode mode)
        {
            ControllerState &state=State();

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

                // base 配方 = 作者原本的配方，不是上一个调试通道的配方：
                // 否则通道之间来回切换会累积（把上一个通道的贴图/渲染状态当成"原始"）。
                const MaterialRecipe *base=saved->had_override?&saved->recipe:nullptr;

                MaterialRecipe debug_recipe;
                if(!BuildDebugViewRecipe(mode,base,debug_recipe))
                    continue;

                material->SetRecipe(debug_recipe);
                ++applied;
            }

            state.world=world;

            GLogInfo(u8"[DebugView] channel=%s primitives=%u/%u",
                     GetDebugViewModeName(mode),
                     applied,
                     static_cast<uint32>(primitives.GetCount()));
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

        {
            hgl::MutexLock lock(&state.lock);

            if(state.requested==state.applied&&state.world==world)
                return false;   // 最常见路径：一次比较就返回，不遍历

            want=state.requested;
        }

        // 切换前先把旧记录按旧世界还原（换世界时 state.world != world）。
        DropAllRecords(state.world);

        if(want!=DebugViewMode::Off)
            ApplyMode(world,want);

        {
            hgl::MutexLock lock(&state.lock);
            state.applied=want;
            state.tracked_count=static_cast<uint32>(state.saved.GetCount());
        }

        return true;
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
