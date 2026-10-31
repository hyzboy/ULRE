/**
 * TestAssetFactory —— 资产工厂：自注册、降级、批量加载。
 *
 * 覆盖点：
 *   · 登记 / 注销 / 计数
 *   · 已注册类型走加载器
 *   · 加载器失败 ⇒ ParseFailed 且结果为空
 *   · 未注册类型 ⇒ Fallback + GenericAssetObject（只记 URI）
 *   · allow_fallback=false ⇒ 空指针 + Fallback（供「必须真读到」的场合）
 *   · default_type 补齐省略类型段的 URI
 *   · 批量加载顺序对齐、单项失败不中断
 */

#include<hgl/asset/AssetFactory.h>

#include<hgl/log/Log.h>
#include<hgl/log/Logger.h>

#include<hgl/type/String.h>

using namespace hgl;
using namespace hgl::asset;

DEFINE_LOGGER_MODULE(TestAssetFactory)
USE_MODULE_LOGGER(TestAssetFactory)

namespace
{
    int g_pass = 0;
    int g_fail = 0;

    void Check(bool ok, const char *what)
    {
        if (ok)
        {
            g_pass++;
        }
        else
        {
            g_fail++;
            MLogError(TestAssetFactory, u8"断言失败：%s", what);
        }
    }

    AnsiString Narrow(const OSString &s)
    {
        return AssetURISegmentToAnsi(s);
    }

    // ── 供测试用的假加载器 ──────────────────────────────────────────────────

    class FakeObject : public AssetObject
    {
        AnsiString name;

    public:

        explicit FakeObject(const AnsiString &n) : name(n) {}

        const ClassMetaInfo *GetClassMetaInfo() const override { return nullptr; }
        const AnsiString    &GetAssetName()     const override { return name; }
    };

    int g_loader_calls = 0;

    bool LoadGeometryOK(const AssetURI &uri, AssetObjectPtr &out, AnsiString &error)
    {
        g_loader_calls++;
        out = AssetObjectPtr(new FakeObject(AssetURISegmentToAnsi(uri.target)));
        (void)error;
        return true;
    }

    bool LoadTextureFail(const AssetURI &uri, AssetObjectPtr &out, AnsiString &error)
    {
        g_loader_calls++;
        (void)uri;
        out = AssetObjectPtr();
        error = "纹理读取失败（测试用）";
        return false;
    }

    // ── 用例 ────────────────────────────────────────────────────────────────

    void TestRegister()
    {
        AssetFactory &f = AssetFactory::Instance();

        f.Clear();
        Check(f.GetRegisteredCount() == 0, "Clear 后计数应为 0");
        Check(!f.IsRegistered(AssetType::Geometry), "未登记时 IsRegistered 应为 false");

        Check(f.Register(AssetType::Geometry, &LoadGeometryOK), "登记 Geometry 应成功");
        Check(f.IsRegistered(AssetType::Geometry), "登记后 IsRegistered 应为 true");
        Check(f.GetRegisteredCount() == 1, "计数应为 1");

        // 重复登记覆盖旧值，不涨计数
        Check(f.Register(AssetType::Geometry, &LoadGeometryOK), "重复登记应成功");
        Check(f.GetRegisteredCount() == 1, "重复登记计数仍为 1");

        // 非法类型 / 空函数
        Check(!f.Register(AssetType::Unknown, &LoadGeometryOK), "Unknown 类型不应可登记");
        Check(!f.Register(AssetType::Geometry, nullptr), "空加载器不应可登记");

        Check(f.Unregister(AssetType::Geometry), "注销应成功");
        Check(!f.IsRegistered(AssetType::Geometry), "注销后 IsRegistered 应为 false");
        Check(f.GetRegisteredCount() == 0, "注销后计数应为 0");

        f.Clear();
    }

    void TestRegisteredPath()
    {
        AssetFactory &f = AssetFactory::Instance();
        f.Clear();
        f.Register(AssetType::Geometry, &LoadGeometryOK);

        g_loader_calls = 0;

        AssetLoadStatus status = AssetLoadStatus::ParseFailed;
        AssetObjectPtr obj = f.Create(OS_TEXT("geo:builtin:cube"), AssetLoadOptions(), &status);

        Check(obj, "已注册类型应返回对象");
        Check(status == AssetLoadStatus::Ok, "状态应为 Ok");
        Check(g_loader_calls == 1, "加载器应被调用一次");
        Check(!obj->GetClassMetaInfo(), "FakeObject 无元数据");
        Check(obj->GetAssetName() == AnsiString("cube"), "资产名应取 target");

        f.Clear();
    }

    void TestLoaderFailure()
    {
        AssetFactory &f = AssetFactory::Instance();
        f.Clear();
        f.Register(AssetType::Texture, &LoadTextureFail);

        AssetLoadStatus status = AssetLoadStatus::Ok;
        AssetObjectPtr obj = f.Create(OS_TEXT("tex:file:rock.tex"), AssetLoadOptions(), &status);

        Check(!obj, "加载器失败应返回空对象");
        Check(status == AssetLoadStatus::ParseFailed, "状态应为 ParseFailed");

        f.Clear();
    }

    void TestFallback()
    {
        AssetFactory &f = AssetFactory::Instance();
        f.Clear();

        // 完全未注册任何类型
        AssetLoadStatus status = AssetLoadStatus::ParseFailed;
        AssetObjectPtr obj = f.Create(OS_TEXT("scene:file:level.scene.toml"),
                                      AssetLoadOptions(), &status);

        Check(obj, "未注册类型应降级出对象");
        Check(status == AssetLoadStatus::Fallback, "状态应为 Fallback");
        Check(!obj->GetClassMetaInfo(), "降级对象无元数据");
        Check(obj->GetAssetName() == AnsiString("level.scene.toml"), "降级对象名应取 target");

        GenericAssetObject *generic = dynamic_cast<GenericAssetObject *>((AssetObject *)obj);
        Check(generic != nullptr, "应能向下转型为 GenericAssetObject");

        if (generic)
        {
            Check(generic->IsFallback(), "IsFallback 应为 true");
            Check(generic->GetURI().type == AssetType::Scene, "URI 类型应解析为 Scene");
            Check(generic->GetURI().source == AssetSourceKind::File, "URI 来源应解析为 File");
            Check(Narrow(generic->GetURI().raw) == AnsiString("scene:file:level.scene.toml"),
                  "降级对象应保留原始 URI");
        }

        f.Clear();
    }

    void TestFallbackDisabled()
    {
        AssetFactory &f = AssetFactory::Instance();
        f.Clear();

        AssetLoadOptions opt;
        opt.allow_fallback = false;

        AssetLoadStatus status = AssetLoadStatus::Ok;
        AssetObjectPtr obj = f.Create(OS_TEXT("prim:file:a.prim.toml"), opt, &status);

        Check(!obj, "禁止降级时应返回空对象");
        Check(status == AssetLoadStatus::Fallback, "状态仍应如实报 Fallback");

        f.Clear();
    }

    void TestUnregisteredTypeName()
    {
        AssetFactory &f = AssetFactory::Instance();
        f.Clear();

        AssetLoadOptions opt;
        opt.allow_fallback = false;

        // 未注册的拼写（"foo" 不是已知类型段）：应走到降级分支
        AssetObjectPtr obj = f.Create(OS_TEXT("foo:file:bar.dat"), opt);

        Check(!obj, "未注册拼写的类型应走降级分支");

        // allow_fallback=true 时降级对象保留原拼写
        AssetLoadOptions opt2;
        AssetObjectPtr obj2 = f.Create(OS_TEXT("foo:file:bar.dat"), opt2);

        GenericAssetObject *generic = dynamic_cast<GenericAssetObject *>((AssetObject *)obj2);
        Check(generic != nullptr, "未注册拼写应降级");

        if (generic)
        {
            Check(generic->GetURI().type == AssetType::Unknown, "type 应为 Unknown");
            Check(generic->GetURI().type_name == AnsiString("foo"), "type_name 应保留原拼写");
            Check(generic->GetURI().source == AssetSourceKind::File, "source 仍应识别出来");
        }

        f.Clear();
    }

    void TestDefaultType()
    {
        AssetFactory &f = AssetFactory::Instance();
        f.Clear();
        f.Register(AssetType::Texture, &LoadTextureFail);

        g_loader_calls = 0;

        // `file:rock.tex` 首段是 source 不是 type —— 类型由 default_type 补齐
        AssetLoadOptions opt;
        opt.default_type = AssetType::Texture;

        AssetLoadStatus status = AssetLoadStatus::Fallback;
        AssetObjectPtr obj = f.Create(OS_TEXT("file:rock.tex"), opt, &status);

        Check(!obj, "假纹理加载器必定失败");
        Check(status == AssetLoadStatus::ParseFailed, "应进入已注册的纹理加载器（ParseFailed）");
        Check(g_loader_calls == 1, "default_type 应把 URI 路由到纹理加载器");

        // 不给 default_type 时同一个 URI 会落到降级
        AssetLoadStatus status2 = AssetLoadStatus::Ok;
        AssetObjectPtr obj2 = f.Create(OS_TEXT("file:rock.tex"), AssetLoadOptions(), &status2);

        Check(obj2, "无 default_type 时应降级");
        Check(status2 == AssetLoadStatus::Fallback, "无 default_type 时状态应为 Fallback");

        f.Clear();
    }

    void TestBatch()
    {
        AssetFactory &f = AssetFactory::Instance();
        f.Clear();
        f.Register(AssetType::Geometry, &LoadGeometryOK);
        f.Register(AssetType::Texture, &LoadTextureFail);

        const OSString uris[] =
        {
            OS_TEXT("geo:builtin:cube"),
            OS_TEXT("tex:file:rock.tex"),      // 失败
            OS_TEXT("rt:Offscreen:color0"),    // 未注册类型 ⇒ 降级
            OS_TEXT("geo:builtin:sphere"),
        };

        AssetObjectPtr list[4];
        uint32_t ok = f.Create(uris, 4, list);

        Check(ok == 3, "成功数应为 3（两几何 + 一降级）");

        Check(list[0] && list[0]->GetAssetName() == AnsiString("cube"), "第 0 项应为 cube");
        Check(!list[1], "第 1 项（纹理失败）应为空");
        Check(list[2] != nullptr, "第 2 项（降级）应存在");
        Check(list[3] && list[3]->GetAssetName() == AnsiString("sphere"), "第 3 项应为 sphere");

        // 空输入
        Check(f.Create(nullptr, 0, nullptr) == 0, "空批量应返回 0");

        // 只数不取：out_objects 传 nullptr
        Check(f.Create(uris, 4, nullptr) == 3, "out_objects=nullptr 时仍应返回成功数");

        f.Clear();
    }

    void TestBatchOverwrites()
    {
        AssetFactory &f = AssetFactory::Instance();
        f.Clear();
        f.Register(AssetType::Geometry, &LoadGeometryOK);

        AssetObjectPtr list[2];
        const OSString first[] = { OS_TEXT("geo:builtin:cube"), OS_TEXT("geo:builtin:cube") };
        Check(f.Create(first, 2, list) == 2, "首次批量应为 2 项成功");

        // 更短的批次：只覆写前 1 项，不得残留旧内容
        const OSString second[] = { OS_TEXT("geo:builtin:dome") };
        Check(f.Create(second, 1, list) == 1, "再次批量应为 1 项成功");
        Check(list[0] && list[0]->GetAssetName() == AnsiString("dome"), "第 0 项被覆写为 dome");

        f.Clear();
    }
}//namespace

int main(int, char **)
{
    hgl::logger::InitLogger(OS_TEXT("TestAssetFactory"));

    TestRegister();
    TestRegisteredPath();
    TestLoaderFailure();
    TestFallback();
    TestFallbackDisabled();
    TestUnregisteredTypeName();
    TestDefaultType();
    TestBatch();
    TestBatchOverwrites();

    if (g_fail == 0)
    {
        MLogInfo(TestAssetFactory, u8"=== TestAssetFactory: %d 项全部通过 ===", g_pass);
    }
    else
    {
        MLogError(TestAssetFactory, u8"=== TestAssetFactory: %d 项通过，%d 项失败 ===", g_pass, g_fail);
    }

    return g_fail == 0 ? 0 : 1;
}