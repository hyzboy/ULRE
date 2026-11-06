// TestSceneLoadPlan — 阶段三判据：从场景包 + 配方文本派生出可执行的任务单
//
// 不用真实 `res/`：这里在临时目录里现搓一个最小 minipack 场景包和几份配方，
// 保证测试自足，也不受示例资产改名影响。
//
// minipack 容器格式（`MiniPackInfoBlock.cpp`）：
//   [8]  "MiniPack"
//   [u32] info_size
//   ---- info block ----
//   [u32] version(=1)  [u32] count
//   [u8]  name_length[count]
//   [char] name[i] + '\0'
//   [u32] offset[count]        ← 相对数据块起点
//   [u32] length[count]
//   ---- data block ----
//   [bytes] 各条目数据

#include <hgl/graph/asset/AssetLoadGraph.h>
#include <hgl/graph/asset/SceneLoadPlan.h>
#include <hgl/graph/mesh/ScenePackFormat.h>
#include <hgl/filesystem/FileSystem.h>
#include <hgl/io/FileOutputStream.h>
#include <hgl/utf.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>

#include <cstring>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::graph::scenepack;

namespace
{
    constexpr const char *kTempDirPath = "ulre_loadplan_test_tmp/";
    constexpr const char *kFullScenePath     = "ulre_loadplan_test_tmp/Stage.Scene.scene";
    constexpr const char *kDegradedScenePath = "ulre_loadplan_test_tmp/Lonely.Scene.scene";
    constexpr const char *kChessRecipe = "ulre_loadplan_test_tmp/LoadScene.Chess.Chessboard.recipe.toml";
    constexpr const char *kLitRecipe   = "ulre_loadplan_test_tmp/LoadScene.Chess.Lit.recipe.toml";
    constexpr const char *kGarbagePath = "ulre_loadplan_test_tmp/Garbage.scene";

    // ── 内存中的 SCN2 载荷构造（与 TestSceneDependencyExtractor 同构） ──────

    struct PayloadBuilder
    {
        ValueArray<uint8_t>        bytes;
        ValueArray<SceneTableDesc> dir;

        void AddTable(SceneTableType type, const void *data, uint32_t size)
        {
            while(bytes.GetCount() % 8)
                bytes.Add(static_cast<uint8_t>(0));

            SceneTableDesc desc;
            desc.type   = static_cast<uint32_t>(type);
            desc.offset = static_cast<uint32_t>(bytes.GetCount());
            desc.size   = size;
            desc.stride = 0;

            bytes.Add(reinterpret_cast<const uint8_t *>(data), static_cast<int>(size));
            dir.Add(desc);
        }

        void Finish(ScenePackHeader &header)
        {
            header.dir_offset = static_cast<uint32_t>(bytes.GetCount());
            header.dir_count  = static_cast<uint32_t>(dir.GetCount());

            bytes.Add(reinterpret_cast<const uint8_t *>(dir.GetData()),
                      static_cast<int>(dir.GetCount() * static_cast<int>(sizeof(SceneTableDesc))));

            header.payload_size = static_cast<uint32_t>(bytes.GetCount());
        }

        const void *Data() const { return bytes.GetData(); }
        uint32_t    Size() const { return static_cast<uint32_t>(bytes.GetCount()); }
    };

    void AddPoolString(ValueArray<char> &pool, const char *text, uint32_t &offset, uint32_t &length)
    {
        offset = static_cast<uint32_t>(pool.GetCount());
        length = static_cast<uint32_t>(std::strlen(text));
        pool.Add(text, static_cast<int>(length));
    }

    /**
     * 两个材质 + 一个回退到外部文件的几何。
     *
     * 没有 GeometryBlob/GeometryView ⇒ 几何会被当成外部依赖，正好覆盖
     * 「闭包里既有包内表项又有外部文件」的情况。
     *
     * 材质在包里的文件名是 `<Scene>.<idx>.<Name>.material`，名字里带场景段和
     * 序号，`MaterialNameFromPackFile` 要能从里面剥出裸材质名。
     */
    void BuildScenePayload(PayloadBuilder &builder, ScenePackHeader &header,
                           const char *scene_name,
                           const char *material0, const char *material1)
    {
        ValueArray<char> pool;

        const AnsiString file0 = AnsiString(scene_name) + ".0." + material0 + ".material";
        const AnsiString file1 = AnsiString(scene_name) + ".1." + material1 + ".material";
        const AnsiString geof  = AnsiString(scene_name) + ".0.geometry";

        uint32_t lit_off = 0, lit_len = 0, chess_off = 0, chess_len = 0;

        AddPoolString(pool, file0.c_str(), lit_off, lit_len);
        AddPoolString(pool, file1.c_str(), chess_off, chess_len);

        PackedMaterial materials[2];
        std::memset(materials, 0, sizeof(materials));
        materials[0].original_index = 0;
        materials[0].file_offset = lit_off;    materials[0].file_length = lit_len;
        materials[1].original_index = 1;
        materials[1].file_offset = chess_off;  materials[1].file_length = chess_len;

        uint32_t geo_off = 0, geo_len = 0;
        AddPoolString(pool, geof.c_str(), geo_off, geo_len);

        PackedPrimitive primitives[2];
        std::memset(primitives, 0, sizeof(primitives));

        for(int i = 0; i < 2; i++)
        {
            primitives[i].geometry_index = 0;
            primitives[i].material_index = static_cast<uint32_t>(i);
            primitives[i].geometry_file_offset = geo_off;
            primitives[i].geometry_file_length = geo_len;
        }

        std::memset(&header, 0, sizeof(header));
        header.magic           = kScenePackV2Magic;
        header.version_major   = 2;
        header.material_count  = 2;
        header.primitive_count = 2;

        builder.AddTable(SceneTableType::MaterialTable,  materials, sizeof(materials));
        builder.AddTable(SceneTableType::PrimitiveTable, primitives, sizeof(primitives));
        builder.AddTable(SceneTableType::StringPool,     pool.GetData(), pool.GetCount());
        builder.Finish(header);
    }

    bool WriteFile(const char *path, const void *data, int size)
    {
        io::FileOutputStream *fos = io::CreateFileOutputStream(ToOSString(path));

        if(fos == nullptr)
            return false;

        const bool ok = fos->Write(data, size) == size;

        delete fos;
        return ok;
    }

    /// 只支持两个条目的最小 minipack 写出器 —— 场景包正好只需要 header + payload
    bool WriteMiniPack(const char *path,
                       const char *name0, const void *data0, uint32_t length0,
                       const char *name1, const void *data1, uint32_t length1)
    {
        const char    *names[2]   = { name0, name1 };
        const void    *datas[2]   = { data0, data1 };
        const uint32_t lengths[2] = { length0, length1 };

        const uint32_t name_length[2] =
        {
            static_cast<uint32_t>(std::strlen(name0)),
            static_cast<uint32_t>(std::strlen(name1)),
        };

        uint32_t info_size = 4 + 4;                     // version + count

        for(uint32_t i = 0; i < 2; i++)
            info_size += 1 + name_length[i] + 1;

        info_size += 2 * 4;                             // offsets
        info_size += 2 * 4;                             // lengths

        ValueArray<uint8_t> out;

        out.Add(reinterpret_cast<const uint8_t *>("MiniPack"), 8);
        out.Add(reinterpret_cast<const uint8_t *>(&info_size), 4);

        const uint32_t version = 1;
        const uint32_t count   = 2;

        out.Add(reinterpret_cast<const uint8_t *>(&version), 4);
        out.Add(reinterpret_cast<const uint8_t *>(&count), 4);

        for(uint32_t i = 0; i < 2; i++)
        {
            const uint8_t len = static_cast<uint8_t>(name_length[i]);
            out.Add(&len, 1);
        }

        for(uint32_t i = 0; i < 2; i++)
            out.Add(reinterpret_cast<const uint8_t *>(names[i]), static_cast<int>(name_length[i] + 1));

        uint32_t offset = 0;

        for(uint32_t i = 0; i < 2; i++)
        {
            out.Add(reinterpret_cast<const uint8_t *>(&offset), 4);
            offset += lengths[i];
        }

        for(uint32_t i = 0; i < 2; i++)
            out.Add(reinterpret_cast<const uint8_t *>(&lengths[i]), 4);

        for(uint32_t i = 0; i < 2; i++)
            out.Add(reinterpret_cast<const uint8_t *>(datas[i]), static_cast<int>(lengths[i]));

        return WriteFile(path, out.GetData(), out.GetCount());
    }

    constexpr char kChessboardRecipeText[] =
        "schema = 1\n"
        "name = \"Stage.Chessboard\"\n"
        "domain = \"StandardOpaque\"\n"
        "\n"
        "[[textures]]\n"
        "name = \"base_color\"\n"
        "asset = \"Stage.Chessboard_base.Tex2D\"\n"
        "\n"
        "[[textures]]\n"
        "name = \"normal\"\n"
        "asset = \"Stage.Chessboard_normal.Tex2D\"\n";

    constexpr char kLitRecipeText[] =
        "schema = 1\n"
        "name = \"Stage.Lit\"\n"
        "domain = \"StandardOpaque\"\n"
        "\n"
        "[[textures]]\n"
        "name = \"base_color\"\n"
        "asset = \"Stage.White.Tex2D\"\n";

    /// 闭包内每个身份只能出现一次 —— 否则异步派发会把同一份资产装两遍
    bool HasNoDuplicateTask(const SceneLoadPlan &plan)
    {
        UnorderedMap<AnsiString, int> seen;

        for(int i = 0; i < plan.tasks.GetCount(); i++)
        {
            const AnsiString id = plan.tasks[i].key.MakeId();

            if(seen.ContainsKey(id))
            {
                GLogError(u8"任务单出现重复身份：%s", id.c_str());
                return false;
            }

            seen.Add(id, i);
        }

        return true;
    }

    bool HasIdPrefix(const AnsiString &id, const char *prefix)
    {
        const int prefix_len = static_cast<int>(std::strlen(prefix));

        if(id.Length() < prefix_len)
            return false;

        return std::memcmp(id.c_str(), prefix, prefix_len) == 0;
    }

    /// 依赖必须排在依赖它的人之前（`SolveClosure` 的后序出栈即拓扑序）
    bool IndexOfTask(const SceneLoadPlan &plan, const char *id_prefix, int &out_index)
    {
        for(int i = 0; i < plan.tasks.GetCount(); i++)
            if(HasIdPrefix(plan.tasks[i].key.MakeId(), id_prefix))
            {
                out_index = i;
                return true;
            }

        return false;
    }

    /// 目录与配方在整个测试里只写一次：降级用例靠「材质名对不上任何配方」来构造，
    /// 不靠删文件 —— `filesystem::DeleteTree` 只有声明没有实现，链接期会炸。
    bool PrepareSceneDirectory()
    {
        if(!filesystem::MakePath(ToOSString(kTempDirPath)))
        {
            GLogError(u8"创建临时目录失败");
            return false;
        }

        if(!WriteFile(kChessRecipe, kChessboardRecipeText,
                      static_cast<int>(std::strlen(kChessboardRecipeText)))
           || !WriteFile(kLitRecipe, kLitRecipeText, static_cast<int>(std::strlen(kLitRecipeText))))
        {
            GLogError(u8"写出配方失败");
            return false;
        }

        return true;
    }

    /// 写一个两材质场景包
    bool WriteScenePack(const char *scene_path, const char *scene_name,
                        const char *material0, const char *material1)
    {
        PayloadBuilder  builder;
        ScenePackHeader header;

        BuildScenePayload(builder, header, scene_name, material0, material1);

        if(!WriteMiniPack(scene_path,
                          "ScenePackHeader", &header, sizeof(header),
                          "ScenePayload", builder.Data(), builder.Size()))
        {
            GLogError(u8"写出合成场景包失败");
            return false;
        }

        return true;
    }

    /// 正常场景：几何 1 + 材质 2 + 配方 2 + 纹理 3，纹理排在配方之前
    bool TestFullPlan()
    {
        if(!WriteScenePack(kFullScenePath, "Stage", "Lit", "Chessboard"))
            return false;

        SceneLoadPlan plan;
        AnsiString    error;

        if(!BuildSceneLoadPlan(AnsiString(kFullScenePath), plan, nullptr, &error))
        {
            GLogError(u8"装载计划构建失败");
            return false;
        }

        if(!plan.acyclic)
        {
            GLogError(u8"合成场景不应有环");
            return false;
        }

        if(plan.CountOf(AssetDepKind::Material)       != 2
           || plan.CountOf(AssetDepKind::Geometry)       != 1
           || plan.CountOf(AssetDepKind::MaterialRecipe) != 2
           || plan.CountOf(AssetDepKind::Texture)        != 3)
        {
            GLogError(u8"任务构成不符：材质=%d 几何=%d 配方=%d 纹理=%d",
                      plan.CountOf(AssetDepKind::Material),
                      plan.CountOf(AssetDepKind::Geometry),
                      plan.CountOf(AssetDepKind::MaterialRecipe),
                      plan.CountOf(AssetDepKind::Texture));
            return false;
        }

        if(!HasNoDuplicateTask(plan))
            return false;

        // 合成包没有 GeometryView/GeometryBlob，几何走的是外部文件回退路径
        if(plan.embedded_geometry_count != 0)
        {
            GLogError(u8"外部几何包不应报告内嵌几何：%d", plan.embedded_geometry_count);
            return false;
        }

        int tex_index = 0;
        int rcp_index = 0;

        // `MakeId()` 的前缀是**资产类型**名（`tex:` / `rcp:`），不是派发种类名
        if(!IndexOfTask(plan, "tex:", tex_index)
           || !IndexOfTask(plan, "rcp:", rcp_index))
        {
            GLogError(u8"任务单缺少纹理或配方");
            return false;
        }

        if(tex_index > rcp_index)
        {
            GLogError(u8"拓扑序错误：纹理（%d）应排在配方（%d）之前", tex_index, rcp_index);
            return false;
        }

        DumpSceneLoadPlan(AnsiString(kFullScenePath), plan);
        return true;
    }

    /// 配方缺失只降级：材质与几何照旧，纹理缺失但不失败
    bool TestMissingRecipeDegrades()
    {
        // `Ghost` 在设计里没有任何配方文件，`Chessboard` 仍是完好的那份
        if(!WriteScenePack(kDegradedScenePath, "Lonely", "Ghost", "Chessboard"))
            return false;

        SceneLoadPlan plan;
        AnsiString    error;

        if(!BuildSceneLoadPlan(AnsiString(kDegradedScenePath), plan, nullptr, &error))
        {
            GLogError(u8"配方缺失不应导致计划构建失败");
            return false;
        }

        if(plan.CountOf(AssetDepKind::Material)          != 2
           || plan.CountOf(AssetDepKind::MaterialRecipe) != 1
           || plan.CountOf(AssetDepKind::Texture)        != 2)
        {
            GLogError(u8"降级后的任务构成不符：材质=%d 配方=%d 纹理=%d",
                      plan.CountOf(AssetDepKind::Material),
                      plan.CountOf(AssetDepKind::MaterialRecipe),
                      plan.CountOf(AssetDepKind::Texture));
            return false;
        }

        return true;
    }

    /// 坏输入必须失败，而不是返回「零依赖」的假结论
    bool TestBrokenInputRejected()
    {
        SceneLoadPlan plan;
        AnsiString    error;

        if(BuildSceneLoadPlan(AnsiString(), plan, nullptr, &error))
        {
            GLogError(u8"空引用应判失败");
            return false;
        }

        if(BuildSceneLoadPlan(AnsiString("no_such_dir/no_such_scene.scene"), plan, nullptr, &error))
        {
            GLogError(u8"不存在的场景应判失败");
            return false;
        }

        // 不是 minipack 的普通文件
        const char garbage[] = "not a minipack";

        if(!WriteFile(kGarbagePath, garbage, static_cast<int>(std::strlen(garbage))))
            return false;

        if(BuildSceneLoadPlan(AnsiString(kGarbagePath), plan, nullptr, &error))
        {
            GLogError(u8"非 minipack 文件应判失败");
            return false;
        }

        return true;
    }
}

int main(int argc, char **argv)
{
    hgl::logger::InitLogger(OS_TEXT("TestSceneLoadPlan"));

    // 手工验证入口：把一个真实场景引用当参数传进来，只打计划不装载。
    //   TestSceneLoadPlan.exe res/xxx/yyy.Scene.scene
    if(argc > 1 && argv[1] != nullptr)
    {
        SceneLoadPlan plan;
        AnsiString    error;

        if(!BuildSceneLoadPlan(AnsiString(argv[1]), plan, nullptr, &error))
        {
            GLogError(u8"计划构建失败：%s", argv[1]);
            return 1;
        }

        DumpSceneLoadPlan(AnsiString(argv[1]), plan);
        return plan.acyclic ? 0 : 2;
    }

    const bool ok = PrepareSceneDirectory()
                 && TestFullPlan()
                 && TestMissingRecipeDegrades()
                 && TestBrokenInputRejected();

    if(!ok)
    {
        GLogError(u8"=== TestSceneLoadPlan: 测试失败 ===");
        return 1;
    }

    GLogInfo(u8"=== TestSceneLoadPlan: 全部测试通过 ===");
    return 0;
}
