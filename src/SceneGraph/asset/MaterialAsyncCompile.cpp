#include <hgl/graph/asset/MaterialAsyncCompile.h>

#include <hgl/asset/LoadedFileManager.h>
#include <hgl/filesystem/FileSystem.h>
#include <hgl/filesystem/Path.h>
#include <hgl/log/Log.h>
#include <hgl/mtl/MaterialDataBlockFlat.h>

#include <climits>
#include <utility>

namespace hgl::graph::mtl
{
    namespace
    {
        bool IsRecipeFile(const OSString &name)
        {
            return name.EndsWith(OS_TEXT(".recipe.toml"));
        }

        bool IsDataBlockToml(const OSString &name)
        {
            return name.EndsWith(OS_TEXT(".material_data.toml"))
                || name.EndsWith(OS_TEXT(".mtldata.toml"));
        }

        bool IsDataBlockMdb(const OSString &name)
        {
            return name.EndsWith(OS_TEXT(".material_data.mdb"))
                || name.EndsWith(OS_TEXT(".mtldata.mdb"))
                || name.EndsWith(OS_TEXT(".mdb"));
        }

        bool IsDataBlockFlat(const OSString &name)
        {
            return name.EndsWith(OS_TEXT(".material_data.fb"))
                || name.EndsWith(OS_TEXT(".mtldata.fb"));
        }

        bool IsAnyDataBlockFile(const OSString &name)
        {
            return IsDataBlockToml(name) || IsDataBlockMdb(name) || IsDataBlockFlat(name);
        }
    }//namespace

    // ── MaterialRecipeParseTask ──────────────────────────────────────────────

    MaterialRecipeParseTask::MaterialRecipeParseTask(const os_char *label_in,
                                                     const char *content,
                                                     const int content_size)
        : AssetCPUTask(hgl::asset::AssetCPUTaskKind::Custom)
        , label(label_in ? label_in : OS_TEXT("<memory>"))
    {
        if (content && content_size > 0)
            memory.Add(reinterpret_cast<const char *>(content), content_size);
    }

    bool MaterialRecipeParseTask::Execute()
    {
        const MaterialRecipeFileParseResult result =
            ParseMaterialRecipeFile(memory.GetData(), memory.GetCount(), data, &spec);

        parse_ok = (result == MaterialRecipeFileParseResult::OK);

        if (!parse_ok)
        {
            // 解析失败**不是**任务失败：`data.parse_error` 有行号/键/值，主线程要用它写日志。
            error = GetMaterialRecipeFileParseResultName(result);
        }

        return true;
    }

    // ── MaterialDataBlockParseTask ───────────────────────────────────────────

    MaterialDataBlockParseTask::MaterialDataBlockParseTask(const os_char *label_in,
                                                           const uint8 *bytes,
                                                           const uint64 size)
        : AssetCPUTask(hgl::asset::AssetCPUTaskKind::Custom)
        , label(label_in ? label_in : OS_TEXT("<memory>"))
    {
        if (bytes && size > 0)
            memory.Add(bytes, static_cast<int>(size));
    }

    bool MaterialDataBlockParseTask::ExecuteBytes(const uint8 *bytes, const uint64 size)
    {
        if (IsDataBlockToml(label))
        {
            if (size == 0 || size > static_cast<uint64>(INT_MAX))
            {
                error = "invalid recipe/data-block size";
                return false;
            }

            const MaterialDataBlockParseResult result =
                ParseMaterialDataBlockFile(reinterpret_cast<const char *>(bytes),
                                           static_cast<int>(size),
                                           data,
                                           &spec);

            has_spec = true;
            parse_ok = (result == MaterialDataBlockParseResult::OK);

            if (!parse_ok)
            {
                failure_reason = "material data block toml: ";
                failure_reason += GetMaterialDataBlockParseResultName(result);
                if (!data.parse_error.detail.IsEmpty())
                {
                    failure_reason += " (";
                    failure_reason += data.parse_error.detail;
                    failure_reason += ")";
                }
            }

            return true;
        }

        if (IsDataBlockMdb(label))
        {
            AnsiString binary_error;
            if (!ReadMaterialDataBlockBinary(bytes, size, data.block, binary_error))
            {
                failure_reason = "material data block binary: ";
                failure_reason += binary_error;
                return true;    // 建 `Failed` 句柄，工具要看「这个文件坏了」
            }

            if (!data.block.IsValid())
            {
                failure_reason = "material data block binary: payload invalid (row_bytes/row_count)";
                return true;
            }

            parse_ok = true;    // 二进制没有授权形态，`has_spec` 保持 `false`
            return true;
        }

        if (IsDataBlockFlat(label))
        {
            AnsiString flat_error;
            if (!DecodeMaterialDataBlockFlat(bytes, size, spec, &flat_error))
            {
                failure_reason = "material data block flatbuffers: ";
                failure_reason += flat_error;
                return true;
            }

            if (!BuildMaterialDataBlock(spec, data.block))
            {
                failure_reason = "material data block flatbuffers: flatten failed";
                return true;
            }

            has_spec = true;
            parse_ok = true;
            return true;
        }

        error = "not a material data block file (.material_data.toml/.mdb/.fb)";
        return false;
    }

    bool MaterialDataBlockParseTask::Execute()
    {
        return ExecuteBytes(memory.GetData(), static_cast<uint64>(memory.GetCount()));
    }

    // ── MaterialAsyncCompiler ────────────────────────────────────────────────

    MaterialAsyncCompiler::MaterialAsyncCompiler()
        : io_relay(this)
    {
    }

    MaterialAsyncCompiler::~MaterialAsyncCompiler()
    {
        Detach();

        // 池/管理器若已停，未消费的成品还压在信息池里；这里兜底释放。
        inbox.DrainOneBatch([this](const hgl::thread::TaskEvent &event)
        {
            if (event.task)
                delete static_cast<hgl::asset::AssetCPUTask *>(event.task);
        });

        inbox.Close();
    }

    bool MaterialAsyncCompiler::Attach(hgl::asset::AssetCPUTaskPool *task_pool,
                                       const uint32 channel_id_in)
    {
        if (!task_pool || !task_pool->IsRunning())
        {
            GLogError(u8"[MaterialAsyncCompiler] 挂接失败：任务池为空或未启动");
            return false;
        }

        if (pool)
        {
            GLogError(u8"[MaterialAsyncCompiler] 已经挂接在任务池上，不能重复挂接");
            return false;
        }

        pool       = task_pool;
        channel_id = channel_id_in;
        return true;
    }

    bool MaterialAsyncCompiler::Detach()
    {
        if (!pool)
            return true;

        if (pending > 0)
        {
            GLogWarning(u8"[MaterialAsyncCompiler] 摘除时还有 %d 个任务未提交，等主线程 Pump 完再摘",
                        pending);
            return false;
        }

        pool = nullptr;
        return true;
    }

    bool MaterialAsyncCompiler::SubmitTask(hgl::asset::AssetCPUTask *task)
    {
        if (!task)
            return false;

        if (!pool)
        {
            GLogError(u8"[MaterialAsyncCompiler] 未挂接任务池，无法投递");
            delete task;
            return false;
        }

        task->affinity = hgl::asset::ExecutionAffinity::WorkerAny;
        task->receiver = &cpu_receiver;

        if (!pool->Submit(task))
        {
            GLogError(u8"[MaterialAsyncCompiler] 任务池拒绝任务（池已停？）");
            delete task;
            return false;
        }

        ++pending;
        return true;
    }

    bool MaterialAsyncCompiler::SubmitFileToRead(const OSString &path, const bool is_recipe)
    {
        if (path.IsEmpty())
        {
            GLogError(u8"[MaterialAsyncCompiler] %s路径为空",
                      is_recipe ? u8"配方" : u8"数据块");
            return false;
        }

        if (!io_manager)
        {
            GLogError(u8"[MaterialAsyncCompiler] 未挂接 LoadedFileManager，文件投递失败: %ls",
                      path.c_str());
            return false;
        }

        FileReadContext *ctx = new FileReadContext();
        ctx->path      = path;
        ctx->is_recipe = is_recipe;

        const hgl::FileLoadId id = io_manager->Submit(path,
                                                      nullptr,
                                                      hgl::io::AsyncFilePriority::Normal,
                                                      &io_relay,
                                                      nullptr,
                                                      ctx);
        if (id == 0)
        {
            delete ctx;
            GLogError(u8"[MaterialAsyncCompiler] 文件读取投递失败: %ls", path.c_str());
            return false;
        }

        ++pending;
        return true;
    }

    void MaterialAsyncCompiler::FileReadRelay::OnTaskEvent(const hgl::thread::TaskEvent &event)
    {
        // 上下文由投递方new出来、经 task_user_data 原样回投；每个请求至多一个终态事件。
        FileReadContext *ctx = static_cast<FileReadContext *>(event.user_data);

        bool handed_off = false;

        if (event.state == hgl::thread::TaskState::Done && ctx && owner && owner->pool)
        {
            hgl::LoadedFile *file = static_cast<hgl::LoadedFile *>(event.task);

            if (file)
            {
                hgl::asset::AssetCPUTask *task = nullptr;

                if (ctx->is_recipe
                    && file->GetSize() >= 1
                    && file->GetSize() <= static_cast<uint64>(INT_MAX))
                {
                    task = new MaterialRecipeParseTask(
                        ctx->path.c_str(),
                        reinterpret_cast<const char *>(file->GetData()),
                        static_cast<int>(file->GetSize()));
                }
                else if (!ctx->is_recipe && file->GetSize() >= 1)
                {
                    task = new MaterialDataBlockParseTask(
                        ctx->path.c_str(),
                        file->GetData(),
                        file->GetSize());
                }

                if (task)
                {
                    task->affinity = hgl::asset::ExecutionAffinity::WorkerAny;
                    task->receiver = &owner->cpu_receiver;

                    handed_off = owner->pool->Submit(task);

                    if (!handed_off)
                        delete task;
                }

                file->RefDec();     // 中继多持的一次引用，随字节拷贝完成一起还掉
            }
        }

        delete ctx;

        if (handed_off)
            return;     // 在途名额从读盘腿转交解析腿，pending 由解析终态事件结算

        // 读盘失败/取消、或转投未遂：压一个不带任务的中止事件，主线程 Pump 结算
        if (!owner)
            return;

        hgl::thread::TaskEvent out{};
        out.error_code = event.error_code;
        out.state      = (event.state == hgl::thread::TaskState::Cancelled)
                             ? hgl::thread::TaskState::Cancelled
                             : hgl::thread::TaskState::Failed;
        owner->inbox.Post(out);
    }

    bool MaterialAsyncCompiler::SubmitRecipeFile(const OSString &path)
    {
        return SubmitFileToRead(path, true);
    }

    bool MaterialAsyncCompiler::SubmitRecipeSource(const os_char *label,
                                                   const char *content,
                                                   const int content_size)
    {
        return SubmitTask(new MaterialRecipeParseTask(label, content, content_size));
    }

    int MaterialAsyncCompiler::SubmitRecipeDirectoryFiles(const OSString &directory)
    {
        if (!hgl::filesystem::Path(directory).IsDirectory())
        {
            GLogError(u8"[MaterialAsyncCompiler] 配方目录不存在或不可访问: %ls", directory.c_str());
            return -1;
        }

        hgl::ValueArray<hgl::filesystem::FileInfo> file_list;
        const int scan_count = hgl::filesystem::GetFileInfoList(
            file_list, directory, true, true, true);
        if (scan_count < 0)
        {
            GLogError(u8"[MaterialAsyncCompiler] 配方目录扫描失败: %ls (result=%d)",
                      directory.c_str(), scan_count);
            return -1;
        }

        int submitted = 0;
        for (int i = 0; i < file_list.GetCount(); ++i)
        {
            const hgl::filesystem::FileInfo &file = file_list[i];
            if (!file.is_file || !IsRecipeFile(OSString(file.name)))
                continue;

            if (SubmitRecipeFile(OSString(file.fullname)))
                ++submitted;
        }

        return submitted;
    }

    bool MaterialAsyncCompiler::SubmitDataBlockFile(const OSString &path)
    {
        return SubmitFileToRead(path, false);
    }

    bool MaterialAsyncCompiler::SubmitDataBlockSource(const os_char *label,
                                                      const uint8 *bytes,
                                                      const uint64 size)
    {
        return SubmitTask(new MaterialDataBlockParseTask(label, bytes, size));
    }

    int MaterialAsyncCompiler::SubmitDataBlockDirectoryFiles(const OSString &directory)
    {
        if (!hgl::filesystem::Path(directory).IsDirectory())
        {
            GLogError(u8"[MaterialAsyncCompiler] 数据块目录不存在或不可访问: %ls", directory.c_str());
            return -1;
        }

        hgl::ValueArray<hgl::filesystem::FileInfo> file_list;
        const int scan_count = hgl::filesystem::GetFileInfoList(
            file_list, directory, true, true, true);
        if (scan_count < 0)
        {
            GLogError(u8"[MaterialAsyncCompiler] 数据块目录扫描失败: %ls (result=%d)",
                      directory.c_str(), scan_count);
            return -1;
        }

        int submitted = 0;
        for (int i = 0; i < file_list.GetCount(); ++i)
        {
            const hgl::filesystem::FileInfo &file = file_list[i];
            if (!file.is_file || !IsAnyDataBlockFile(OSString(file.name)))
                continue;

            if (SubmitDataBlockFile(OSString(file.fullname)))
                ++submitted;
        }

        return submitted;
    }

    void MaterialAsyncCompiler::HandleTaskEvent(const hgl::thread::TaskEvent &event,
                                                MaterialRecipeRegistry *recipes,
                                                MaterialDataBlockRegistry *blocks,
                                                hgl::graph::TextureManager *texture_manager,
                                                int &committed)
    {
        // 每个在途名额恰好对应一个终态事件：进门先结算
        if (pending > 0)
            --pending;

        hgl::asset::AssetCPUTask *task = static_cast<hgl::asset::AssetCPUTask *>(event.task);

        if (!task)
        {
            // 读盘腿中止（文件读不到 / 已取消 / 转投未遂）：没有任务体，直接计数
            if (event.state == hgl::thread::TaskState::Cancelled)
                ++cancelled;
            else
                ++failed;
            return;
        }

        if (task->IsCancelled())
        {
            ++cancelled;
            delete task;
            return;
        }

        if (!task->IsDone())
        {
            GLogError(u8"[MaterialAsyncCompiler] 任务失败: %s", task->error.c_str());
            ++failed;
            delete task;
            return;
        }

        if (MaterialRecipeParseTask *recipe_task =
                dynamic_cast<MaterialRecipeParseTask *>(task))
        {
            if (!recipe_task->IsParseOk())
            {
                const MaterialRecipeFileParseError &parse_error =
                    recipe_task->GetData().parse_error;

                GLogError(u8"[MaterialAsyncCompiler] 配方解析失败: %ls line=%d table=%s key=%s value=\"%s\" detail=%s",
                          recipe_task->GetLabel().c_str(),
                          parse_error.line,
                          parse_error.table.c_str(),
                          parse_error.key.c_str(),
                          parse_error.value.c_str(),
                          parse_error.detail.c_str());

                ++failed;
                delete task;
                return;
            }

            if (!recipes)
            {
                GLogWarning(u8"[MaterialAsyncCompiler] 收到配方但没有注册表，丢弃: %ls",
                            recipe_task->GetLabel().c_str());
                ++failed;
                delete task;
                return;
            }

            if (recipes->RegisterParsedRecipe(recipe_task->GetLabel().c_str(),
                                              recipe_task->GetData(),
                                              std::move(recipe_task->GetSpec()),
                                              texture_manager))
            {
                ++recipe_done;
                ++committed;
            }
            else
            {
                ++failed;
            }

            delete task;
            return;
        }

        if (MaterialDataBlockParseTask *block_task =
                dynamic_cast<MaterialDataBlockParseTask *>(task))
        {
            if (!blocks)
            {
                GLogWarning(u8"[MaterialAsyncCompiler] 收到数据块但没有注册表，丢弃: %ls",
                            block_task->GetLabel().c_str());
                ++failed;
                delete task;
                return;
            }

            MaterialDataBlockAsset *asset =
                blocks->CommitParsed(std::move(block_task->GetData()),
                                     std::move(block_task->GetSpec()),
                                     block_task->HasSpec(),
                                     block_task->GetLabel().c_str(),
                                     block_task->GetFailureReason());

            if (asset && block_task->IsParseOk())
            {
                ++block_done;
                ++committed;
            }
            else
            {
                ++failed;
            }

            delete task;
            return;
        }

        // 不是本编译器投递的任务类型：不认领，直接释放（任务串了才会走到这里）
        GLogWarning(u8"[MaterialAsyncCompiler] 收到未知任务类型，已释放");
        ++failed;
        delete task;
    }

    int MaterialAsyncCompiler::Pump(MaterialRecipeRegistry *recipes,
                                    MaterialDataBlockRegistry *blocks,
                                    hgl::graph::TextureManager *texture_manager)
    {
        int committed = 0;

        // 一次锁交换处理整批：工作线程一口气跑完 N 个解析也只换一次
        inbox.DrainOneBatch([&](const hgl::thread::TaskEvent &event)
        {
            HandleTaskEvent(event, recipes, blocks, texture_manager, committed);
        });

        return committed;
    }
}//namespace hgl::graph::mtl
