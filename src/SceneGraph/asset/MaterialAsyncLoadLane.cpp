#include <hgl/graph/asset/MaterialAsyncLoadLane.h>

#include <hgl/graph/asset/MaterialDataBlockRegistry.h>
#include <hgl/graph/module/TextureManager.h>
#include <hgl/mtl/MaterialRecipeRegistry.h>

#include <hgl/asset/AssetCPUTaskPool.h>
#include <hgl/log/Log.h>

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

        const int submitted = compiler->SubmitRecipeDirectoryFiles(directory);

        if (submitted > 0)
            submitted_recipes += submitted;

        return submitted;
    }

    int MaterialAsyncLoadLane::BeginDataBlockDirectory(const OSString &directory)
    {
        if (!EnsureStarted())
            return -1;

        const int submitted = compiler->SubmitDataBlockDirectoryFiles(directory);

        if (submitted > 0)
            submitted_blocks += submitted;

        return submitted;
    }

    bool MaterialAsyncLoadLane::BeginRecipeFile(const OSString &path)
    {
        if (!EnsureStarted())
            return false;

        if (!compiler->SubmitRecipeFile(path))
            return false;

        ++submitted_recipes;
        return true;
    }

    bool MaterialAsyncLoadLane::BeginDataBlockFile(const OSString &path)
    {
        if (!EnsureStarted())
            return false;

        if (!compiler->SubmitDataBlockFile(path))
            return false;

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
