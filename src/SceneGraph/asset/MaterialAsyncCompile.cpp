#include <hgl/graph/asset/MaterialAsyncCompile.h>

#include <hgl/filesystem/FileSystem.h>
#include <hgl/filesystem/Path.h>
#include <hgl/io/FileInputStream.h>
#include <hgl/log/Log.h>
#include <hgl/mtl/MaterialDataBlockFlat.h>
#include <hgl/type/Smart.h>

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

        /**
         * 读整个文件到带 `'\0'` 尾巴的缓冲。
         *
         * 尾巴是给 TOML 解析器用的：`ParseMaterialRecipeFile` / `ParseMaterialDataBlockFile`
         * 虽然收长度，但保底有一个终止符更安全（与同步路径 `ReadWholeFile` 一致）。
         */
        bool ReadWholeFileBytes(const OSString &path,
                                AutoDeleteArray<uint8> &out_buffer,
                                int64 &out_size)
        {
            out_size = 0;

            hgl::io::OpenFileInputStream opener(path);
            if (!opener)
            {
                GLogError(u8"[MaterialAsyncCompiler] 文件打不开: %ls", path.c_str());
                return false;
            }

            const int64 size = opener->GetSize();
            if (size <= 0 || size > INT_MAX)
            {
                GLogError(u8"[MaterialAsyncCompiler] 文件大小无效: %ls (%lld)",
                          path.c_str(), static_cast<long long>(size));
                return false;
            }

            if (!out_buffer.alloc(static_cast<size_t>(size) + 1))
            {
                GLogError(u8"[MaterialAsyncCompiler] 文件缓冲分配失败: %ls (%lld)",
                          path.c_str(), static_cast<long long>(size));
                return false;
            }

            if (opener->Read(out_buffer.data(), size) != size)
            {
                GLogError(u8"[MaterialAsyncCompiler] 文件读取失败: %ls", path.c_str());
                return false;
            }

            out_buffer[static_cast<size_t>(size)] = 0;
            out_size = size;
            return true;
        }
    }//namespace

    // ── MaterialRecipeParseTask ──────────────────────────────────────────────

    MaterialRecipeParseTask::MaterialRecipeParseTask(const OSString &path)
        : AssetCPUTask(hgl::asset::AssetCPUTaskKind::Custom)
        , label(path)
    {
    }

    MaterialRecipeParseTask::MaterialRecipeParseTask(const os_char *label_in,
                                                     const char *content,
                                                     const int content_size)
        : AssetCPUTask(hgl::asset::AssetCPUTaskKind::Custom)
        , label(label_in ? label_in : OS_TEXT("<memory>"))
        , file_mode(false)
    {
        if (content && content_size > 0)
            memory.Add(reinterpret_cast<const char *>(content), content_size);

        read_ok = true;     // 字节已经在手上了
    }

    bool MaterialRecipeParseTask::Execute()
    {
        const char *content = nullptr;
        int content_size = 0;

        AutoDeleteArray<uint8> buffer;

        if (file_mode)
        {
            int64 size = 0;
            if (!ReadWholeFileBytes(label, buffer, size))
            {
                error = "cannot read recipe file";
                return false;
            }

            read_ok = true;
            content = reinterpret_cast<const char *>(buffer.data());
            content_size = static_cast<int>(size);
        }
        else
        {
            content = memory.GetData();
            content_size = memory.GetCount();
        }

        const MaterialRecipeFileParseResult result =
            ParseMaterialRecipeFile(content, content_size, data, &spec);

        parse_ok = (result == MaterialRecipeFileParseResult::OK);

        if (!parse_ok)
        {
            // 解析失败**不是**任务失败：`data.parse_error` 有行号/键/值，主线程要用它写日志。
            error = GetMaterialRecipeFileParseResultName(result);
        }

        return true;
    }

    // ── MaterialDataBlockParseTask ───────────────────────────────────────────

    MaterialDataBlockParseTask::MaterialDataBlockParseTask(const OSString &path)
        : AssetCPUTask(hgl::asset::AssetCPUTaskKind::Custom)
        , label(path)
    {
    }

    MaterialDataBlockParseTask::MaterialDataBlockParseTask(const os_char *label_in,
                                                           const uint8 *bytes,
                                                           const uint64 size)
        : AssetCPUTask(hgl::asset::AssetCPUTaskKind::Custom)
        , label(label_in ? label_in : OS_TEXT("<memory>"))
        , file_mode(false)
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
        if (!file_mode)
            return ExecuteBytes(memory.GetData(), static_cast<uint64>(memory.GetCount()));

        AutoDeleteArray<uint8> buffer;
        int64 size = 0;
        if (!ReadWholeFileBytes(label, buffer, size))
        {
            error = "cannot read material data block file";
            return false;
        }

        return ExecuteBytes(buffer.data(), static_cast<uint64>(size));
    }

    // ── MaterialAsyncCompiler::MaterialAsyncBox ──────────────────────────────

    void MaterialAsyncCompiler::MaterialAsyncBox::OnAssetCPUTaskReady(
        hgl::asset::AssetCPUTask *task)
    {
        if (task)
            done.Post(task);
    }

    int MaterialAsyncCompiler::MaterialAsyncBox::Take(
        hgl::ValueArray<hgl::asset::AssetCPUTask *> &out)
    {
        if (!done.TrySwap())
            return 0;

        hgl::ValueArray<hgl::asset::AssetCPUTask *> &batch = done.GetReceive();
        const int count = batch.GetCount();

        if (count > 0)
            out.Add(batch.GetData(), count);

        done.ReleaseReceive();
        return count;
    }

    void MaterialAsyncCompiler::MaterialAsyncBox::Close()
    {
        done.Close();
    }

    // ── MaterialAsyncCompiler ────────────────────────────────────────────────

    MaterialAsyncCompiler::~MaterialAsyncCompiler()
    {
        Detach();

        // 池若已 Stop(CancelPending)，未消费的成品还在邮箱里；这里兜底释放。
        hgl::ValueArray<hgl::asset::AssetCPUTask *> leftover;
        box.Take(leftover);

        for (int i = 0; i < leftover.GetCount(); ++i)
            delete leftover[i];

        box.Close();
    }

    bool MaterialAsyncCompiler::Attach(hgl::asset::AssetCPUTaskPool *task_pool,
                                       const uint32 sink_id_in,
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

        if (task_pool->RegisterSink(sink_id_in, &box) == false)
        {
            GLogError(u8"[MaterialAsyncCompiler] sink 注册失败（编号重复？）: %u", sink_id_in);
            return false;
        }

        pool       = task_pool;
        sink_id    = sink_id_in;
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

        pool->UnregisterSink(sink_id);
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

        task->affinity        = hgl::asset::ExecutionAffinity::WorkerAny;
        task->result_affinity = hgl::asset::ExecutionAffinity::Dedicated;
        task->result_target_id = sink_id;

        if (!pool->Submit(task))
        {
            GLogError(u8"[MaterialAsyncCompiler] 任务池拒绝任务（池已停？）");
            delete task;
            return false;
        }

        ++pending;
        return true;
    }

    bool MaterialAsyncCompiler::SubmitRecipeFile(const OSString &path)
    {
        if (path.IsEmpty())
        {
            GLogError(u8"[MaterialAsyncCompiler] 配方路径为空");
            return false;
        }

        return SubmitTask(new MaterialRecipeParseTask(path));
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
        if (path.IsEmpty())
        {
            GLogError(u8"[MaterialAsyncCompiler] 数据块路径为空");
            return false;
        }

        return SubmitTask(new MaterialDataBlockParseTask(path));
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

    int MaterialAsyncCompiler::Pump(MaterialRecipeRegistry *recipes,
                                    MaterialDataBlockRegistry *blocks,
                                    hgl::graph::TextureManager *texture_manager)
    {
        hgl::ValueArray<hgl::asset::AssetCPUTask *> batch;
        box.Take(batch);

        int committed = 0;

        for (int i = 0; i < batch.GetCount(); ++i)
        {
            hgl::asset::AssetCPUTask *task = batch[i];
            if (!task)
                continue;

            if (pending > 0)
                --pending;

            if (task->IsCancelled())
            {
                ++cancelled;
                delete task;
                continue;
            }

            if (!task->IsDone())
            {
                GLogError(u8"[MaterialAsyncCompiler] 任务失败: %s", task->error.c_str());
                ++failed;
                delete task;
                continue;
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
                    continue;
                }

                if (!recipes)
                {
                    GLogWarning(u8"[MaterialAsyncCompiler] 收到配方但没有注册表，丢弃: %ls",
                                recipe_task->GetLabel().c_str());
                    ++failed;
                    delete task;
                    continue;
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
                continue;
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
                    continue;
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
                continue;
            }

            // 不是本编译器投递的任务类型：不认领，直接释放（sink 编号串了才会走到这里）
            GLogWarning(u8"[MaterialAsyncCompiler] 收到未知任务类型，已释放");
            ++failed;
            delete task;
        }

        return committed;
    }
}//namespace hgl::graph::mtl
