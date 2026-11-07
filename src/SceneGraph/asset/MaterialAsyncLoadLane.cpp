#include <hgl/graph/asset/MaterialAsyncLoadLane.h>

#include <hgl/graph/asset/MaterialDataBlockRegistry.h>
#include <hgl/graph/asset/AssetPathKey.h>
#include <hgl/graph/module/TextureManager.h>
#include <hgl/mtl/MaterialRecipeRegistry.h>

#include <hgl/asset/AssetCPUTaskPool.h>
#include <hgl/log/Log.h>

#include <hgl/filesystem/FileSystem.h>

#include <hgl/thread/ShutdownMode.h>

namespace hgl::graph::mtl
{
    MaterialAsyncLoadLane::~MaterialAsyncLoadLane()
    {
        // 注册表可能已经先走了：这里只保证线程不再跑、池被回收。
        // 成品丢掉是安全的（车道只做「让数据提前就位」这件事）。
        Shutdown(nullptr, nullptr, nullptr);
    }

    bool MaterialAsyncLoadLane::EnsureStarted(int worker_count)
    {
        if (compiler)
            return true;

        if (!pool)
        {
            pool = new hgl::asset::AssetCPUTaskPool();

            if (!pool->Start(worker_count > 0 ? static_cast<hgl::uint>(worker_count) : 2u))
            {
                GLogError(u8"[MaterialAsyncLoadLane] 任务池启动失败");
                delete pool;
                pool = nullptr;
                return false;
            }

            owns_pool = true;
        }

        MaterialAsyncCompiler *created = new MaterialAsyncCompiler();

        if (!created->Attach(pool, kLaneSinkID))
        {
            GLogError(u8"[MaterialAsyncLoadLane] 编译器挂接任务池失败（sink 编号冲突？）");
            delete created;
            return false;
        }

        compiler = created;
        return true;
    }

    void MaterialAsyncLoadLane::Shutdown(MaterialRecipeRegistry *recipes,
                                         MaterialDataBlockRegistry *blocks,
                                         hgl::graph::TextureManager *texture_manager)
    {
        if (!pool)
            return;

        if (compiler)
        {
            // `Drain` 等所有已投递任务跑完并把成品投回邮箱；此时再泵一次是唯一
            // 能把邮箱掏空的时机 —— 掏空之后 `Detach()` 才会成功。
            pool->Stop(hgl::thread::ShutdownMode::Drain);

            pumped_commits += compiler->Pump(recipes, blocks, texture_manager);

            if (!compiler->Detach())
            {
                // 走到这里说明还有任务没被泵掉（例如池在 Drain 期间又收到了新的
                // 提交）。编译器析构会再尝试一次，池随之销毁，不会留下悬垂 sink。
                GLogWarning(u8"[MaterialAsyncLoadLane] 收尾时仍有在途任务，成品可能被丢弃");
            }

            delete compiler;
            compiler = nullptr;
        }
        else
        {
            pool->Stop(hgl::thread::ShutdownMode::Drain);
        }

        if (owns_pool)
        {
            delete pool;
            owns_pool = false;
        }

        pool = nullptr;
    }

    int MaterialAsyncLoadLane::BeginRecipeDirectory(const OSString &directory)
    {
        if (!EnsureStarted())
            return -1;

        hgl::ValueArray<hgl::filesystem::FileInfo> list;

        if (hgl::filesystem::GetFileInfoList(list, directory, false, true, true) < 0)
            return -1;

        const int before = submitted_recipes;

        for (int i = 0; i < list.GetCount(); ++i)
        {
            const hgl::filesystem::FileInfo &file = list[i];

            if (!file.is_file)
                continue;

            const hgl::OSString name(file.name);

            if (!name.EndsWith(OS_TEXT(".recipe.toml")))
                continue;

            BeginRecipeFile(hgl::OSString(file.fullname));
        }

        // 只报「真的投出去」的数量：被去重跳过的文件不算（调用方用它判断门禁要不要等）
        return submitted_recipes - before;
    }

    int MaterialAsyncLoadLane::BeginDataBlockDirectory(const OSString &directory)
    {
        if (!EnsureStarted())
            return -1;

        hgl::ValueArray<hgl::filesystem::FileInfo> list;

        if (hgl::filesystem::GetFileInfoList(list, directory, false, true, true) < 0)
            return -1;

        const int before = submitted_blocks;

        for (int i = 0; i < list.GetCount(); ++i)
        {
            const hgl::filesystem::FileInfo &file = list[i];

            if (!file.is_file)
                continue;

            const hgl::OSString name(file.name);

            if (!name.EndsWith(OS_TEXT(".material_data.toml"))
             && !name.EndsWith(OS_TEXT(".mdb"))
             && !name.EndsWith(OS_TEXT(".fb")))
                continue;

            BeginDataBlockFile(hgl::OSString(file.fullname));
        }

        return submitted_blocks - before;
    }

    bool MaterialAsyncLoadLane::IsInFlight(const OSString &path) const
    {
        const OSString key = MakeAssetPathKey(path);

        for (int i = 0; i < in_flight.GetCount(); ++i)
            if (*in_flight.At(i) == key)
                return true;

        return false;
    }

    void MaterialAsyncLoadLane::MarkInFlight(const OSString &path)
    {
        in_flight.Create(MakeAssetPathKey(path));
    }

    void MaterialAsyncLoadLane::ClearInFlightWhenIdle()
    {
        if (IsIdle())
            in_flight.Clear();
    }

    bool MaterialAsyncLoadLane::BeginRecipeFile(const OSString &path)
    {
        if (!EnsureStarted())
            return false;

        // 已经落地过 ⇒ 视为已就位（不是失败）
        if (dedup_recipes && dedup_recipes->FindAssetByPath(path))
        {
            ++skipped_existing;
            return true;
        }

        if (IsInFlight(path))
        {
            ++skipped_existing;
            return true;
        }

        if (!compiler->SubmitRecipeFile(path))
            return false;

        MarkInFlight(path);
        ++submitted_recipes;
        return true;
    }

    bool MaterialAsyncLoadLane::BeginDataBlockFile(const OSString &path)
    {
        if (!EnsureStarted())
            return false;

        if (dedup_blocks && dedup_blocks->FindAssetByPath(path))
        {
            ++skipped_existing;
            return true;
        }

        if (IsInFlight(path))
        {
            ++skipped_existing;
            return true;
        }

        if (!compiler->SubmitDataBlockFile(path))
            return false;

        MarkInFlight(path);
        ++submitted_blocks;
        return true;
    }

    int MaterialAsyncLoadLane::Pump(MaterialRecipeRegistry *recipes,
                                    MaterialDataBlockRegistry *blocks,
                                    hgl::graph::TextureManager *texture_manager)
    {
        if (!compiler)
            return 0;

        const int committed = compiler->Pump(recipes, blocks, texture_manager);

        if (committed > 0)
            pumped_commits += committed;

        ClearInFlightWhenIdle();

        return committed;
    }

    bool MaterialAsyncLoadLane::IsIdle() const
    {
        return compiler ? compiler->IsIdle() : true;
    }

    int MaterialAsyncLoadLane::GetPendingCount() const
    {
        return compiler ? compiler->GetPendingCount() : 0;
    }

    int MaterialAsyncLoadLane::GetFailedCount() const
    {
        return compiler ? compiler->GetFailedCount() : 0;
    }
}//namespace hgl::graph::mtl
