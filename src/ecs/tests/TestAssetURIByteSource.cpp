/**
 * TestAssetURIByteSource —— 包裹取流统一进 LoadedFileManager 后的回归测试。
 *
 * 前身是 TestPackReadTask（CPU 池取流通道，已随阶段四退役）。覆盖四件事：
 *   1. ResolveAssetPhysicalPath() 在目录型包裹里先查索引：带类型段的逻辑路径
 *      要落到索引裁决出的真实文件（.geo.fb），id:<32hex> 要能反查出路径，
 *      索引里没有的原始素材才退回直通拼接；
 *   2. GetAssetMountRoot() 能取回挂载根（供字节源做物理盘路由）；
 *   3. LoadedFileManager::Submit + AssetURIByteSource：URI 原文当查表键、
 *      字节由 IO 线程经 OpenAssetStream 分派取回，产出与磁盘内容逐字节相等；
 *   4. 失败路径不静默：取不到的 URI 必须回调 OnFileFailed；同一 URI 的
 *      重复提交合并成一次读盘，两个请求都拿到回调。
 */
#include <hgl/graph/asset/AssetSourceMount.h>

#include <hgl/asset/AssetByteSource.h>
#include <hgl/asset/AssetIndex.h>
#include <hgl/asset/AssetSource.h>
#include <hgl/asset/AssetURI.h>
#include <hgl/asset/LoadedFile.h>
#include <hgl/asset/LoadedFileManager.h>
#include <hgl/asset/LoadedFileHandler.h>

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
        GLogError(u8"TestAssetURIByteSource 失败: %s", label);
    }

    constexpr const char *kPackName = "packbytetest";
    constexpr const char *kTempRoot = "build/test_packbytes";
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

    /**
     * 极简回调记录器：LoadedFileManager 只会在调用线程（Pump/Stop）派发，
     * 不需要引用计数与原子。
     */
    class Recorder final : public LoadedFileHandler
    {
    public:

        bool loaded   = false;
        bool failed   = false;
        bool notified = false;

        ValueArray<uint8> bytes;

        void OnFileLoaded(const FileLoadTicket &, LoadedFile *file) override
        {
            loaded   = true;
            notified = true;

            if (file)
            {
                const uint64 size = file->GetSize();

                if (size > 0 && size < static_cast<uint64>(INT32_MAX))
                {
                    const uint8 *src = static_cast<const uint8 *>(file->GetData());

                    bytes.Resize(static_cast<int>(size));

                    for (uint64 i = 0; i < size; ++i)
                        bytes[static_cast<int>(i)] = src[i];
                }
            }
        }

        void OnFileFailed(const FileLoadTicket &, io::AsyncFileLoadError) override
        {
            failed   = true;
            notified = true;
        }

        void OnFileCancelled(const FileLoadTicket &) override
        {
            notified = true;
        }
    };

    void WaitUntil(LoadedFileManager &manager, Recorder **recs, int count)
    {
        for (int i = 0; i < 5000; ++i)
        {
            manager.Pump();

            bool all = true;

            for (int n = 0; n < count; ++n)
                if (!recs[n]->notified)
                    all = false;

            if (all)
                return;

            hgl::SleepSecond(0.001);
        }
    }

    /// 提交一个「URI 原文 + 字节源」请求并等回调落地；返回记录器（调用方持有）
    Recorder *RunOneRead(LoadedFileManager &manager, const OSString &uri_text,
                         asset::AssetType default_type)
    {
        const asset::AssetURI uri = asset::AssetURI::Parse(uri_text, default_type);

        OSString route;
        graph::GetAssetMountRoot(uri.pack_name, route);

        Recorder *rec = new Recorder();

        const FileLoadId id = manager.Submit(uri_text, rec, io::AsyncFilePriority::Normal,
                                             nullptr,
                                             new asset::AssetURIByteSource(route, uri));

        if (id == 0)
        {
            Check(false, "Submit 不应拒绝合法请求");
            delete rec;
            return nullptr;
        }

        WaitUntil(manager, &rec, 1);

        return rec;
    }
}//namespace

int main()
{
    hgl::logger::InitLogger(OS_TEXT("TestAssetURIByteSource"));

    if (!PrepareTree())
    {
        GLogError(u8"准备测试素材失败");
        return 1;
    }

    graph::AssetSourceMountConfig config;

    config.name          = AnsiString(kPackName);
    config.root          = ToOSString(kTempRoot);
    config.build_index   = true;
    config.load_manifest = false;
    config.save_manifest = false;
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

        OSString raw_path;

        Check(graph::ResolveAssetPhysicalPath(MakeURI(ToOSString(kRawFile)), raw_path),
              "索引外的原始素材退回直通拼接");
        Check(ToU8String(raw_path).EndsWith(raw_suffix), "原始素材路径正确");
    }

    // ── 2. id 身份寻址反查 + 挂载根查询 ───────────────────────────────────
    {
        asset::AssetSource *source = asset::GetSource(AnsiString(kPackName));

        Check(source != nullptr, "能取到刚挂载的包裹来源");
        Check(source != nullptr && source->HasIndexTable(), "包裹必须建了索引");

        OSString root;

        Check(graph::GetAssetMountRoot(AnsiString(kPackName), root) && !root.IsEmpty(),
              "挂载根必须能查到");
        Check(!graph::GetAssetMountRoot(AnsiString("NoSuchPack"), root),
              "未挂载的包裹名必须返回 false");

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

        OSString path;

        Check(!graph::ResolveAssetPhysicalPath(MakeURI(OS_TEXT("id:00000000000000000000000000000000")),
                                               asset::AssetType::Unknown,
                                               path),
              "不存在的 id 必须解析失败");
        Check(path.IsEmpty(), "失败时输出路径必须被清空");
    }

    // ── 3. LoadedFileManager + 字节源的取流链路 ───────────────────────────
    LoadedFileManager manager;

    Check(manager.Start(1), "读盘管理器启动");

    {
        Recorder *geo = RunOneRead(manager, MakeURI(OS_TEXT("models/hero")),
                                   asset::AssetType::Geometry);

        Check(geo != nullptr, "取流请求必须能取回回调");

        if (geo)
        {
            Check(geo->loaded && !geo->failed, "存在的资产必须成功");
            Check(geo->bytes.GetCount() == static_cast<int>(strlen(kGeoBytes))
                      && memcmp(geo->bytes.GetData(), kGeoBytes, geo->bytes.GetCount()) == 0,
                  "取到的字节必须逐字节相等（且是索引裁决的 .fb）");

            delete geo;
        }

        Recorder *raw = RunOneRead(manager, MakeURI(ToOSString(kRawFile)),
                                  asset::AssetType::Unknown);

        Check(raw != nullptr, "原始素材取流必须能取回回调");

        if (raw)
        {
            Check(raw->loaded && !raw->failed, "索引外的原始素材必须成功");
            Check(raw->bytes.GetCount() == static_cast<int>(strlen(kRawBytes))
                      && memcmp(raw->bytes.GetData(), kRawBytes, raw->bytes.GetCount()) == 0,
                  "原始素材字节必须逐字节相等");

            delete raw;
        }

        Recorder *bad = RunOneRead(manager, MakeURI(OS_TEXT("models/not_exist_at_all")),
                                  asset::AssetType::Geometry);

        Check(bad != nullptr, "失败请求同样要投递回调");

        if (bad)
        {
            Check(bad->failed && !bad->loaded, "不存在的引用必须回调 OnFileFailed");
            Check(bad->bytes.GetCount() == 0, "失败时不得留下缓冲");

            delete bad;
        }
    }

    // ── 4. 同一 URI 重复提交：合并成一次读盘，两边都有回调 ────────────────
    {
        const OSString uri_text = MakeURI(OS_TEXT("models/hero"));
        const asset::AssetURI uri = asset::AssetURI::Parse(uri_text, asset::AssetType::Geometry);

        OSString root;
        graph::GetAssetMountRoot(AnsiString(kPackName), root);

        Recorder *a = new Recorder();
        Recorder *b = new Recorder();

        const FileLoadId id_a = manager.Submit(uri_text, a, io::AsyncFilePriority::Normal,
                                               nullptr, new asset::AssetURIByteSource(root, uri));
        const FileLoadId id_b = manager.Submit(uri_text, b, io::AsyncFilePriority::Normal,
                                               nullptr, new asset::AssetURIByteSource(root, uri));

        Check(id_a != 0 && id_b != 0 && id_a != id_b, "两个请求各自拿到独立票据");

        Recorder *pair[2] = { a, b };

        WaitUntil(manager, pair, 2);

        Check(a->loaded && b->loaded, "合并读盘的两个请求都要成功");
        Check(a->bytes.GetCount() == static_cast<int>(strlen(kGeoBytes))
                  && b->bytes.GetCount() == static_cast<int>(strlen(kGeoBytes)),
              "两边字节都要完整");

        // 已发布进缓存后再提交：当场命中，不产生第二次读盘
        Recorder *c = new Recorder();

        const FileLoadId id_c = manager.Submit(uri_text, c, io::AsyncFilePriority::Normal);

        Check(id_c != 0, "缓存命中的提交要有票据");

        manager.Pump();

        Check(c->loaded, "缓存命中必须在派发轮里直接成功");

        delete a;
        delete b;
        delete c;
    }

    manager.Stop(thread::ShutdownMode::Drain);
    manager.Collect();

    graph::UnmountAssetSource(AnsiString(kPackName));

    if (g_fail > 0 || g_pass == 0)
    {
        GLogError(u8"=== TestAssetURIByteSource: 通过 %d / %d 项，%d 项失败 ===",
                  g_pass, g_pass + g_fail, g_fail);
        return 1;
    }

    GLogInfo(u8"=== TestAssetURIByteSource: 全部 %d 项测试通过 ===", g_pass);
    return 0;
}
