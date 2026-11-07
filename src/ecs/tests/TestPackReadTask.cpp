/**
 * TestPackReadTask —— 包裹寻址两条新通道的回归测试。
 *
 * 覆盖三件事：
 *   1. `ResolveAssetPhysicalPath()` 在目录型包裹里**先查索引**：带类型段的逻辑路径
 *      要落到索引裁决出的真实文件（`.geo.fb`），`id:<32 hex>` 要能反查出路径，
 *      索引里没有的原始素材才退回直通拼接；
 *   2. `PackReadTask` 在 CPU 工作线程上取流，产出的字节与磁盘内容逐字节相等；
 *      失败时 `Execute()` 返回 false 且 `error` 非空（不能静默给出空缓冲）；
 *   3. 经 `AssetCPUTaskPool` 提交 → `Collect()` 取回的异步链路可用，且取回的
 *      任务状态为 `Done`。
 *
 * 本测试**不**验证性能，只验证「从主线程同步读搬到工作线程之后语义没变」。
 */
#include <hgl/graph/asset/AssetSourceMount.h>
#include <hgl/graph/asset/PackReadTask.h>

#include <hgl/asset/AssetCPUTaskPool.h>
#include <hgl/asset/AssetIndex.h>
#include <hgl/asset/AssetSource.h>

#include <hgl/filesystem/FileSystem.h>
#include <hgl/filesystem/Path.h>
#include <hgl/io/FileOutputStream.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>
#include <hgl/time/Time.h>
#include <hgl/type/ValueArray.h>
#include <hgl/utf.h>

#include <cstring>

using namespace hgl;

namespace
{
    int g_pass = 0;
    int g_fail = 0;

    void Check(const bool ok, const char *label)
    {
        if (ok)
        {
            ++g_pass;
            return;
        }

        ++g_fail;
        GLogError(u8"TestPackReadTask 失败: %s", label);
    }

    constexpr const char *kPackName = "packreadtest";
    constexpr const char *kTempRoot = "build/test_packread";   ///< 落 build/（已被忽略），不污染仓库根

    constexpr const char *kGeoFile  = "models/hero.geo.fb";
    constexpr const char *kGeoBytes = "HELLO-GEO-FB";
    constexpr const char *kRawFile  = "data/raw.bin";
    constexpr const char *kRawBytes = "RAWBYTES";

    bool WriteFile(const OSString &path, const char *text)
    {
        io::FileOutputStream *fos = io::CreateFileOutputStream(path);

        if (fos == nullptr)
            return false;

        const int size = static_cast<int>(strlen(text));

        const bool ok = (fos->Write(text, size) == size);

        delete fos;

        return ok;
    }

    OSString PackPath(const char *rel)
    {
        OSString path = ToOSString(kTempRoot);

        path += OS_TEXT("/");
        path += ToOSString(rel);

        return path;
    }

    /// 写入一份测试素材树：一份带类型后缀的烘焙产物，一份原始二进制
    bool PrepareTree()
    {
        if (!filesystem::MakePath(ToOSString(kTempRoot))
            || !filesystem::MakePath(PackPath("models"))
            || !filesystem::MakePath(PackPath("data")))
        {
            GLogError(u8"创建临时目录失败");
            return false;
        }

        return WriteFile(PackPath(kGeoFile), kGeoBytes)
            && WriteFile(PackPath(kRawFile), kRawBytes);
    }

    OSString MakeURI(const OSString &target)
    {
        OSString uri = OS_TEXT("pack(");
        uri += ToOSString(kPackName);
        uri += OS_TEXT("):");
        uri += target;

        return uri;
    }

    /// 提交一个取流任务并等它回来；返回任务指针（调用方 delete），超时返回 nullptr
    graph::PackReadTask *RunOneTask(asset::AssetCPUTaskPool &pool, const OSString &uri)
    {
        graph::PackReadTask *task = new graph::PackReadTask(uri, asset::AssetType::Geometry);

        if (!pool.Submit(task))
        {
            delete task;
            return nullptr;
        }

        hgl::ValueArray<asset::AssetCPUTask *> done;

        for (int i = 0; i < 10000; ++i)
        {
            if (pool.Collect(done) > 0 && done.GetCount() > 0)
                break;

            hgl::SleepSecond(0.001);
        }

        if (done.GetCount() != 1)
        {
            for (asset::AssetCPUTask *t : done)
                delete t;

            return nullptr;
        }

        // 池归本测试所有，里面只可能是 PackReadTask。
        return static_cast<graph::PackReadTask *>(done[0]);
    }
}//namespace

int main()
{
    hgl::logger::InitLogger(OS_TEXT("TestPackReadTask"));

    if (!PrepareTree())
    {
        GLogError(u8"准备测试素材失败");
        return 1;
    }

    graph::AssetSourceMountConfig config;

    config.name          = AnsiString(kPackName);
    config.root          = ToOSString(kTempRoot);
    config.build_index   = true;
    config.load_manifest = false;   ///< 目录是临时造的，没有清单可读
    config.save_manifest = false;   ///< 运行期绝不回写清单
    config.only_read     = true;

    AnsiString mount_error;

    if (!graph::MountAssetSource(config, &mount_error))
    {
        GLogError(u8"挂载临时包裹失败: %s", mount_error.c_str());
        return 1;
    }

    const U8String geo_suffix = ToU8String(ToOSString(kGeoFile));
    const U8String raw_suffix = ToU8String(ToOSString(kRawFile));

    // ── 1. 索引优先的物理路径解析 ─────────────────────────────────────────
    {
        OSString path;

        Check(graph::ResolveAssetPhysicalPath(MakeURI(OS_TEXT("models/hero")),
                                              asset::AssetType::Geometry,
                                              path),
              "带类型段的逻辑路径必须能解析成物理路径");
        Check(ToU8String(path).EndsWith(geo_suffix),
              "解析结果必须是索引裁决出的 .fb，而不是拼出来的名字");
        Check(filesystem::FileExist(path), "解析出的物理路径必须真实存在");

        // 原始素材：索引里没有条目，必须退回直通拼接
        OSString raw_path;

        Check(graph::ResolveAssetPhysicalPath(MakeURI(ToOSString(kRawFile)), raw_path),
              "索引外的原始素材退回直通拼接");
        Check(ToU8String(raw_path).EndsWith(raw_suffix), "原始素材路径正确");
    }

    // ── 2. `id:` 身份寻址反查 ─────────────────────────────────────────────
    {
        asset::AssetSource *source = asset::GetSource(AnsiString(kPackName));

        Check(source != nullptr, "能取到刚挂载的包裹来源");
        Check(source != nullptr && source->HasIndexTable(), "包裹必须建了索引");

        asset::AssetIndexEntry entry;

        const bool found = (source != nullptr)
                        && source->GetIndexTable() != nullptr
                        && source->GetIndexTable()->FindByPath(asset::AssetType::Geometry,
                                                              ToU8String(OS_TEXT("models/hero")),
                                                              entry);

        Check(found, "索引里能按 (类型, 逻辑路径) 找到几何");

        if (found)
        {
            OSString path;

            Check(graph::ResolveAssetPhysicalPath(MakeURI(ToOSString(asset::MakeAssetIDTarget(entry.id))),
                                                  asset::AssetType::Unknown,
                                                  path),
                  "id 寻址必须能反查出物理路径");
            Check(ToU8String(path).EndsWith(geo_suffix), "id 反查结果正确");
        }

        // 不存在的 ID：必须失败，不能摸到别的文件
        OSString path;

        Check(!graph::ResolveAssetPhysicalPath(MakeURI(OS_TEXT("id:00000000000000000000000000000000")),
                                               asset::AssetType::Unknown,
                                               path),
              "不存在的 id 必须解析失败");
        Check(path.IsEmpty(), "失败时输出路径必须被清空");
    }

    // ── 3. CPU 池上的取流任务 ─────────────────────────────────────────────
    {
        asset::AssetCPUTaskPool pool;

        Check(pool.Start(2), "CPU 池启动");

        graph::PackReadTask *task = RunOneTask(pool, MakeURI(OS_TEXT("models/hero")));

        Check(task != nullptr, "异步取流任务必须能取回");

        if (task)
        {
            Check(task->IsDone(), "任务状态必须是 Done");
            Check(task->GetDataSize() == static_cast<int64>(strlen(kGeoBytes)),
                  "取到的字节数必须与磁盘文件一致");
            Check(task->GetData() != nullptr
                      && memcmp(task->GetData(), kGeoBytes, static_cast<size_t>(task->GetDataSize())) == 0,
                  "取到的字节必须逐字节相等");

            delete task;
        }

        // 打不开的引用：必须失败并带诊断，而不是给一个空缓冲
        graph::PackReadTask *bad = RunOneTask(pool, MakeURI(OS_TEXT("models/not_exist_at_all")));

        Check(bad != nullptr, "失败任务同样要投递回主线程");

        if (bad)
        {
            Check(bad->IsFailed(), "不存在的引用必须失败");
            Check(!bad->error.IsEmpty(), "失败必须带诊断文案");
            Check(bad->GetData() == nullptr && bad->GetDataSize() == 0, "失败时不得留下缓冲");

            delete bad;
        }

        pool.Stop(hgl::thread::ShutdownMode::Drain);
    }

    graph::UnmountAssetSource(AnsiString(kPackName));

    if (g_fail > 0 || g_pass == 0)
    {
        GLogError(u8"=== TestPackReadTask: 通过 %d / %d 项，%d 项失败 ===",
                  g_pass, g_pass + g_fail, g_fail);
        return 1;
    }

    GLogInfo(u8"=== TestPackReadTask: 全部 %d 项测试通过 ===", g_pass);
    return 0;
}
