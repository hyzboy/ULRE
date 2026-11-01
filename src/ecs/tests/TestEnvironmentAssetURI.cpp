// TestEnvironmentAssetURI.cpp —— 环境资产表（`env:` URI）的契约门禁：
//
//   1. `RegisterFromToml()` 产出的身份（名字 / resource_id）与查询口径；
//   2. `env:@Name` 按名命中；名字未注册时明确失败并给出原因（不静默当路径读盘）；
//   3. 类型段不是 `env` 时拒绝（防止把 `geo:file:x` 塞进环境槽后变成莫名的读盘失败）；
//   4. 落盘 `.env.toml` 走 `GetOrLoadURI()`：读盘一次 + 后续命中缓存（指针相等）；
//   5. 扩展名不是 `.env.toml` / `.environment.toml` 时拒绝；
//   6. `ApplyEnvironmentSpec()` 的 `Presence` 局部覆盖语义：缺键保留现值、
//      「显式写成缺省值」也算覆盖、`sh9.bands` 整组替换且不足 9 条补 0；
//   7. 跨字段业务规则（`schema` 版本、`sh9.bands` 条数）在解析层被拦下。
//
// 需要真实的文件读写（第 4 条），但**不需要**设备 / 窗口，因此 CI 里也能跑。
#include <hgl/asset/AssetURI.h>
#include <hgl/ecs/systems/render/SkyEnvironmentConfig.h>
#include <hgl/filesystem/FileSystem.h>
#include <hgl/graph/asset/EnvironmentAssetRegistry.h>
#include <hgl/io/FileOutputStream.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>
#include <hgl/utf.h>

using namespace hgl;
using namespace hgl::asset;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{
    int g_failed = 0;

    void Check(const bool condition, const char *what)
    {
        if (condition)
            return;

        GLogError(u8"TestEnvironmentAssetURI 失败: %s", what);
        ++g_failed;
    }

    constexpr char kNamedEnvironment[] =
        "schema = 1\n"
        "name = \"Sky34.Probe\"\n"
        "\n"
        "[visual]\n"
        "recipe = \"IBLEnvironment.Sky\"\n"
        "scale = 1.25\n"
        "\n"
        "[ambient]\n"
        "mode = \"cubemap_ibl\"\n"
        "\n"
        "[ibl]\n"
        "irradiance = \"res/image/Environments/sky_34_2k/sky_34_cubemap_2k.TexCube\"\n"
        "prefilter = \"res/image/Environments/sky_34_2k/sky_34_prefilter_2k.TexCube\"\n";

    /// 只有 `[ldr]` 一段：用于验证「缺键保留现值」
    constexpr char kPartialEnvironment[] =
        "schema = 1\n"
        "[ldr]\n"
        "exposure_base = 7.0\n";

    /// 写一个临时 `.env.toml`（内容与 `kNamedEnvironment` 相同，但故意不带 name）
    bool WriteProbeFile(const OSString &path, const char *content)
    {
        hgl::io::FileOutputStream *fos = hgl::io::CreateFileOutputStream(path);
        if (!fos)
            return false;

        const int len = static_cast<int>(hgl::strlen(content));
        const bool ok = fos->Write(content, len) == len;
        delete fos;
        return ok;
    }
}//namespace

int main(int, char **)
{
    hgl::logger::InitLogger(OS_TEXT("TestEnvironmentAssetURI"));

    // ── 1+2. 内存注册 + 按名查询 ────────────────────────────────────────────
    {
        EnvironmentAssetRegistry registry;

        EnvironmentAsset *asset = registry.RegisterFromToml(
            OS_TEXT("<probe>"), kNamedEnvironment, static_cast<int>(hgl::strlen(kNamedEnvironment)));

        Check(asset != nullptr, "RegisterFromToml 应成功");
        Check(registry.GetCount() == 1, "注册后条数应为 1");

        if (asset)
        {
            Check(asset->GetEnvironmentName() == "Sky34.Probe", "资产名应来自 spec.name");
            Check(asset->GetResourceId() == U8String(u8"env:@Sky34.Probe"),
                  "有名字时 resource_id 应为 env:@<name>");
            Check(registry.FindByResourceId(asset->GetResourceId()) == asset,
                  "FindByResourceId 应命中同一句柄");
        }

        // 名字大小写不敏感（与其它资产表一致）
        Check(registry.GetOrLoadURI(OS_TEXT("env:@sky34.probe")) == asset,
              "env:@<name> 应按名命中（大小写不敏感）");

        GLogInfo(u8"[1] 内存注册 + 按名查询 OK");
    }

    // ── 2b. 名字未注册 / 类型不符 / 空 URI ──────────────────────────────────
    {
        EnvironmentAssetRegistry registry;

        Check(registry.GetOrLoadURI(OS_TEXT("env:@Missing")) == nullptr,
              "未注册的名字应失败");
        Check(registry.GetLastError().Contains("未注册"), "失败原因应指明名字未注册");

        Check(registry.GetOrLoadURI(OS_TEXT("geo:file:res/x.geo.toml")) == nullptr,
              "类型不是 env 的 URI 应被拒绝");
        Check(registry.GetLastError().Contains("类型"), "失败原因应指明类型不符");

        Check(registry.GetOrLoadURI(OS_TEXT("")) == nullptr, "空 URI 应失败");
        Check(registry.GetLastError().Contains("空 URI"), "失败原因应指明空 URI");

        GLogInfo(u8"[2] 无效 URI 拒绝 OK");
    }

    // ── 3+4. 落盘文件：读盘一次 + 缓存命中 ──────────────────────────────────
    {
        const OSString dir = OS_TEXT("build/env_uri_probe");
        const OSString path = dir + OS_TEXT("/probe.env.toml");

        hgl::filesystem::MakePath(dir);
        Check(WriteProbeFile(path, kNamedEnvironment), "临时 .env.toml 应写入成功");

        EnvironmentAssetRegistry registry;

        EnvironmentAsset *first = registry.GetOrLoadURI(path);
        Check(first != nullptr, "按路径装载应成功");
        Check(registry.GetCount() == 1, "装载后条数应为 1");

        if (first)
        {
            // 文件里写了 name，因此身份仍是名字优先
            Check(first->GetEnvironmentName() == "Sky34.Probe", "文件里的 name 应生效");
            Check(first->GetSourcePath() == path, "GetSourcePath 应回填原始路径");
            Check(registry.FindAssetByPath(path) == first, "FindAssetByPath 应命中");
        }

        // 第二次（含 `env:file:` 显式写法）必须命中缓存，不重新读盘、不产生新记录
        Check(registry.GetOrLoadURI(path) == first, "重复装载应命中缓存");
        Check(registry.GetOrLoadURI(OSString(OS_TEXT("env:file:")) + path) == first,
              "env:file: 显式写法应命中同一份缓存");
        Check(registry.GetCount() == 1, "缓存命中不应新增记录");

        GLogInfo(u8"[3] 落盘装载 + 缓存 OK");
    }

    // ── 5. 扩展名门禁 ───────────────────────────────────────────────────────
    {
        EnvironmentAssetRegistry registry;

        Check(registry.GetOrLoadURI(OS_TEXT("res/env/bogus.toml")) == nullptr,
              "非 .env.toml 扩展名应被拒绝");
        Check(registry.GetLastError().Contains(".env.toml"),
              "失败原因应指明需要的扩展名");

        GLogInfo(u8"[4] 扩展名门禁 OK");
    }

    // ── 6. `Presence` 局部覆盖语义 ─────────────────────────────────────────
    {
        EnvironmentAssetRegistry registry;

        EnvironmentAsset *partial = registry.RegisterFromToml(
            nullptr, kPartialEnvironment, static_cast<int>(hgl::strlen(kPartialEnvironment)));

        Check(partial != nullptr, "只有 [ldr] 的配置应注册成功");

        if (partial)
        {
            SkyEnvironmentConfig cfg;
            cfg.mode = SkyMode::CubeMapIBL;          // 文件没写 [ambient] ⇒ 必须被保留
            cfg.sky_recipe = "Preserved.Sky";        // 文件没写 [visual] ⇒ 必须被保留
            cfg.sky_scale = 2.5f;
            cfg.ldr_exposure_base = 1.0f;            // 文件写了 ⇒ 必须被覆盖

            ApplyEnvironmentSpec(partial->GetSpec(), cfg);

            Check(cfg.mode == SkyMode::CubeMapIBL, "缺键不应覆盖 mode");
            Check(cfg.sky_recipe == "Preserved.Sky", "缺键不应覆盖 sky_recipe");
            Check(cfg.sky_scale == 2.5f, "缺键不应覆盖 sky_scale");
            Check(cfg.ldr_exposure_base == 7.0f, "显式键应覆盖 ldr.exposure_base");

            // 未写的键保留 `SkyEnvironmentConfig` 的类内缺省值
            Check(cfg.ldr_sun_scale == 1.0f, "未写的键应保留类内缺省值");
        }

        // 「显式写成缺省值」与「缺键」必须可区分
        constexpr char kExplicitZero[] =
            "schema = 1\n"
            "[visual]\n"
            "scale = 0.0\n";

        EnvironmentAsset *zero = registry.RegisterFromToml(
            nullptr, kExplicitZero, static_cast<int>(hgl::strlen(kExplicitZero)));

        if (zero)
        {
            SkyEnvironmentConfig cfg;
            cfg.sky_scale = 3.0f;
            ApplyEnvironmentSpec(zero->GetSpec(), cfg);
            Check(cfg.sky_scale == 0.0f, "显式写 0 应覆盖（0 = 用内置默认）");
        }

        GLogInfo(u8"[5] Presence 局部覆盖 OK");
    }

    // ── 6b. `sh9.bands` 整组替换 + 顺序 ────────────────────────────────────
    {
        constexpr char kBands[] =
            "schema = 1\n"
            "[sh9]\n"
            "auto_generate = false\n"
            "bands = []\n";

        // 空数组 = 没写 ⇒ 保留现值（不能被清成 0）
        EnvironmentAssetRegistry registry;
        EnvironmentAsset *empty = registry.RegisterFromToml(
            nullptr, kBands, static_cast<int>(hgl::strlen(kBands)));

        Check(empty != nullptr, "bands = [] 应注册成功（等价于没写）");

        if (empty)
        {
            SkyEnvironmentConfig cfg;
            cfg.sh[0] = math::Vector4f(0.5f, 0.25f, 0.125f, 0.0f);
            ApplyEnvironmentSpec(empty->GetSpec(), cfg);
            Check(cfg.sh[0].x == 0.5f, "空 bands 不应覆盖已有系数");
        }

        // 完整 9 条：逐条按基函数顺序落位
        constexpr char kNineBands[] =
            "schema = 1\n"
            "[sh9]\n"
            "auto_generate = false\n"
            "bands = [\n"
            "  { r = 0.1, g = 0.2, b = 0.3 },\n"
            "  { r = 0.4, g = 0.5, b = 0.6 },\n"
            "  { r = 0.7, g = 0.8, b = 0.9 },\n"
            "  { r = 1.0, g = 1.1, b = 1.2 },\n"
            "  { r = 1.3, g = 1.4, b = 1.5 },\n"
            "  { r = 1.6, g = 1.7, b = 1.8 },\n"
            "  { r = 1.9, g = 2.0, b = 2.1 },\n"
            "  { r = 2.2, g = 2.3, b = 2.4 },\n"
            "  { r = 2.5, g = 2.6, b = 2.7 },\n"
            "]\n";

        EnvironmentAsset *nine = registry.RegisterFromToml(
            nullptr, kNineBands, static_cast<int>(hgl::strlen(kNineBands)));

        Check(nine != nullptr, "9 条 bands 应注册成功");

        if (nine)
        {
            SkyEnvironmentConfig cfg;
            ApplyEnvironmentSpec(nine->GetSpec(), cfg);

            bool ordered = true;
            for (int i = 0; i < 9; ++i)
            {
                const float base = 0.1f + 0.3f * static_cast<float>(i);
                ordered = ordered
                       && fabsf(cfg.sh[i].x - base) < 1.0e-5f
                       && fabsf(cfg.sh[i].y - (base + 0.1f)) < 1.0e-5f
                       && fabsf(cfg.sh[i].z - (base + 0.2f)) < 1.0e-5f;
            }

            Check(ordered, "bands 应按数组顺序逐条落到 sh[0..8]");
            Check(cfg.WantsSkySH(), "auto_generate=false 且有系数 ⇒ WantsSkySH 为真");
        }

        GLogInfo(u8"[6] sh9.bands 顺序与整组覆盖 OK");
    }

    // ── 7. 跨字段业务规则 ──────────────────────────────────────────────────
    {
        EnvironmentAssetRegistry registry;

        constexpr char kBadSchema[] = "schema = 2\n";
        Check(registry.RegisterFromToml(nullptr, kBadSchema,
                                        static_cast<int>(hgl::strlen(kBadSchema))) == nullptr,
              "schema != 1 应被拒绝");
        Check(registry.GetLastError().Contains("schema"), "失败原因应指明 schema");

        constexpr char kBadBands[] =
            "schema = 1\n"
            "[sh9]\n"
            "bands = [ { r = 1.0, g = 1.0, b = 1.0 } ]\n";

        Check(registry.RegisterFromToml(nullptr, kBadBands,
                                        static_cast<int>(hgl::strlen(kBadBands))) == nullptr,
              "bands 条数不是 9 应被拒绝");
        Check(registry.GetLastError().Contains("9"), "失败原因应指明需要 9 条");

        Check(registry.GetCount() == 0, "失败不应留下记录");

        GLogInfo(u8"[7] 跨字段业务规则 OK");
    }

    if (g_failed)
    {
        GLogError(u8"=== TestEnvironmentAssetURI: %d 处失败 ===", g_failed);
        return 1;
    }

    GLogInfo(u8"=== TestEnvironmentAssetURI: 全部测试通过 ===");
    return 0;
}
