// TestModelDependencyExtractor — 阶段三判据：不装载也能抽出模型/配方依赖
//
// 全部用内存里现搓的 MDL2 包与配方文本，不读盘、不建设备。

#include <hgl/graph/asset/AssetLoadGraph.h>
#include <hgl/graph/asset/ModelDependencyExtractor.h>
#include <hgl/graph/mesh/ModelPackFormat.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>

#include <cstring>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::graph::modelpack;

namespace
{
    /// 按 MDL2 布局把若干表串成 payload，并在末尾追加表目录
    struct PayloadBuilder
    {
        ValueArray<uint8_t>        bytes;
        ValueArray<ModelTableDesc> dir;

        void AddTable(ModelTableType type, const void *data, uint32_t size)
        {
            // 表内是 POD 强转访问，入口先对齐到 8 字节
            while(bytes.GetCount() % 8)
                bytes.Add(static_cast<uint8_t>(0));

            ModelTableDesc desc;
            desc.type   = static_cast<uint32_t>(type);
            desc.offset = static_cast<uint32_t>(bytes.GetCount());
            desc.size   = size;
            desc.stride = 0;

            bytes.Add(reinterpret_cast<const uint8_t *>(data), static_cast<int>(size));
            dir.Add(desc);
        }

        /// 追加目录，填好包头里与目录相关的三个字段
        void Finish(ModelPackHeader &header)
        {
            header.dir_offset = static_cast<uint32_t>(bytes.GetCount());
            header.dir_count  = static_cast<uint32_t>(dir.GetCount());

            bytes.Add(reinterpret_cast<const uint8_t *>(dir.GetData()),
                      static_cast<int>(dir.GetCount() * static_cast<int>(sizeof(ModelTableDesc))));

            header.payload_size = static_cast<uint32_t>(bytes.GetCount());
        }

        const void *Data() const { return bytes.GetData(); }
        uint32_t    Size() const { return static_cast<uint32_t>(bytes.GetCount()); }
    };

    void InitHeader(ModelPackHeader &header, uint32_t materials, uint32_t primitives)
    {
        std::memset(&header, 0, sizeof(header));
        header.magic          = kModelPackV2Magic;
        header.version_major  = 2;
        header.version_minor  = 0;
        header.material_count = materials;
        header.primitive_count = primitives;
    }

    /// StringPool 里一段字符串的登记，返回 PackedMaterial 需要的 offset/length
    void AddPoolString(ValueArray<char> &pool, const char *text, uint32_t &offset, uint32_t &length)
    {
        offset = static_cast<uint32_t>(pool.GetCount());
        length = static_cast<uint32_t>(std::strlen(text));
        pool.Add(text, static_cast<int>(length));
    }

    int CountKind(const AssetLoadGraph &graph, const AssetDepKey &root, AssetDepKind kind)
    {
        int count = 0;
        const AssetDepEdge *edges = graph.GetDependencies(root);

        if(!edges)
            return 0;

        const int total = graph.GetDependencyCount(root);

        for(int i = 0; i < total; i++)
            if(edges[i].kind == kind)
                count++;

        return count;
    }

    /// 模型包：2 个材质 + 1 个回退几何（没有 GeometryBlob/GeometryView）
    bool BuildLegacyModelPack(PayloadBuilder &builder, ModelPackHeader &header)
    {
        ValueArray<char> pool;

        uint32_t m0_off = 0, m0_len = 0, m1_off = 0, m1_len = 0;
        AddPoolString(pool, "Scene.0.Lit.material", m0_off, m0_len);
        AddPoolString(pool, "Scene.1.Chessboard.material", m1_off, m1_len);

        PackedMaterial mats[2];
        std::memset(mats, 0, sizeof(mats));
        mats[0].original_index = 0; mats[0].file_offset = m0_off; mats[0].file_length = m0_len;
        mats[1].original_index = 1; mats[1].file_offset = m1_off; mats[1].file_length = m1_len;

        // 同一个几何被两个图元引用：依赖必须去重成 1 条边
        uint32_t g_off = 0, g_len = 0;
        AddPoolString(pool, "Scene.0.geometry", g_off, g_len);

        PackedPrimitive prims[2];
        std::memset(prims, 0, sizeof(prims));
        prims[0].geometry_index = 0; prims[0].material_index = 0;
        prims[0].geometry_file_offset = g_off; prims[0].geometry_file_length = g_len;
        prims[1].geometry_index = 0; prims[1].material_index = 1;
        prims[1].geometry_file_offset = g_off; prims[1].geometry_file_length = g_len;

        InitHeader(header, 2, 2);

        builder.AddTable(ModelTableType::MaterialTable, mats, sizeof(mats));
        builder.AddTable(ModelTableType::PrimitiveTable, prims, sizeof(prims));
        builder.AddTable(ModelTableType::StringPool, pool.GetData(), static_cast<uint32_t>(pool.GetCount()));
        builder.Finish(header);

        return true;
    }

    bool TestLegacyModelDependencies()
    {
        PayloadBuilder  builder;
        ModelPackHeader header;

        if(!BuildLegacyModelPack(builder, header))
            return false;

        AssetLoadGraph graph;
        int edges = -1;

        if(!ExtractModelDependencies(&header, sizeof(header), builder.Data(), builder.Size(),
                                     AnsiString("ABeautifulGame.StaticMesh/"),
                                     AnsiString("ABeautifulGame.StaticMesh/ABeautifulGame.model"),
                                     graph, &edges))
        {
            GLogError(u8"提取依赖失败");
            return false;
        }

        // 2 材质 + 1 几何（两个图元引用同一几何 → 去重）
        if(edges != 3)
        {
            GLogError(u8"边数不符，期望 3 实际 %d", edges);
            return false;
        }

        AssetDepKey root;
        NormalizeAssetRef(AnsiString("ABeautifulGame.StaticMesh/ABeautifulGame.model"),
                          asset::AssetType::Model, root);

        if(CountKind(graph, root, AssetDepKind::Material) != 2
           || CountKind(graph, root, AssetDepKind::Geometry) != 1)
        {
            GLogError(u8"依赖种类分布不符");
            return false;
        }

        // 基准目录参与了规范化：包内相对名必须带上前缀
        const AssetDepEdge *list = graph.GetDependencies(root);

        if(!list || list[0].target.type != asset::AssetType::MaterialRecipe
                 || std::strcmp(list[0].target.path, "abeautifulgame.staticmesh/scene.0.lit") != 0)
        {
            GLogError(u8"材质依赖身份不符");
            return false;
        }

        // 整体替换语义：同一场景再抽一次，边数不能累加
        if(!ExtractModelDependencies(&header, sizeof(header), builder.Data(), builder.Size(),
                                     AnsiString("ABeautifulGame.StaticMesh/"),
                                     AnsiString("ABeautifulGame.StaticMesh/ABeautifulGame.model"),
                                     graph, &edges)
           || edges != 3 || graph.GetDependencyCount(root) != 3)
        {
            GLogError(u8"重复提取导致依赖累加");
            return false;
        }

        return true;
    }

    bool TestEmbeddedGeometryIsNotADependency()
    {
        ValueArray<char> pool;

        uint32_t m_off = 0, m_len = 0;
        AddPoolString(pool, "Scene.0.Lit.material", m_off, m_len);
        uint32_t g_off = 0, g_len = 0;
        AddPoolString(pool, "Scene.0.geometry", g_off, g_len);

        PackedMaterial mat;
        std::memset(&mat, 0, sizeof(mat));
        mat.file_offset = m_off;
        mat.file_length = m_len;

        PackedPrimitive prim;
        std::memset(&prim, 0, sizeof(prim));
        prim.geometry_index = 0;
        prim.geometry_file_offset = g_off;
        prim.geometry_file_length = g_len;

        PackedGeometryView view;
        std::memset(&view, 0, sizeof(view));
        view.blob_offset = 0;
        view.blob_size   = 64;

        uint8_t blob[64] = {};

        PayloadBuilder   builder;
        ModelPackHeader  header;
        InitHeader(header, 1, 1);

        builder.AddTable(ModelTableType::MaterialTable, &mat, sizeof(mat));
        builder.AddTable(ModelTableType::PrimitiveTable, &prim, sizeof(prim));
        builder.AddTable(ModelTableType::GeometryViewTable, &view, sizeof(view));
        builder.AddTable(ModelTableType::GeometryBlob, blob, sizeof(blob));
        builder.AddTable(ModelTableType::StringPool, pool.GetData(), static_cast<uint32_t>(pool.GetCount()));
        builder.Finish(header);

        AssetLoadGraph graph;
        int edges = -1;

        if(!ExtractModelDependencies(&header, sizeof(header), builder.Data(), builder.Size(),
                                     AnsiString("m/"), AnsiString("m/s.model"), graph, &edges))
            return false;

        // 几何内嵌在包里 → 只剩材质这一条外部依赖
        if(edges != 1)
        {
            GLogError(u8"内嵌几何被误判成外部依赖，边数 %d", edges);
            return false;
        }

        return true;
    }

    bool TestBrokenPackRejected()
    {
        PayloadBuilder  builder;
        ModelPackHeader header;

        // 缺 StringPool 的包不是「零依赖」，而是坏包
        PackedMaterial mat;
        std::memset(&mat, 0, sizeof(mat));

        InitHeader(header, 1, 0);
        builder.AddTable(ModelTableType::MaterialTable, &mat, sizeof(mat));
        builder.Finish(header);

        AssetLoadGraph graph;

        if(ExtractModelDependencies(&header, sizeof(header), builder.Data(), builder.Size(),
                                    AnsiString(), AnsiString("s.model"), graph, nullptr))
        {
            GLogError(u8"缺 StringPool 的包应判失败");
            return false;
        }

        // 载荷长度撒谎同样要判失败
        PayloadBuilder  builder2;
        ModelPackHeader header2;
        InitHeader(header2, 0, 0);
        builder2.Finish(header2);
        header2.payload_size += 4096;

        if(ExtractModelDependencies(&header2, sizeof(header2), builder2.Data(), builder2.Size(),
                                    AnsiString(), AnsiString("s.model"), graph, nullptr))
        {
            GLogError(u8"载荷长度越界应判失败");
            return false;
        }

        // 魔数不对
        ModelPackHeader bad;
        std::memset(&bad, 0, sizeof(bad));
        bad.magic = 0x12345678u;
        bad.version_major = 2;

        if(ExtractModelDependencies(&bad, sizeof(bad), builder2.Data(), builder2.Size(),
                                    AnsiString(), AnsiString("s.model"), graph, nullptr))
        {
            GLogError(u8"魔数不符应判失败");
            return false;
        }

        return true;
    }

    constexpr char kRecipeWithDeps[] =
        "schema = 1\n"
        "name = \"Test.Dep\"\n"
        "domain = \"UnlitTexture\"\n"
        "\n"
        "[transform]\n"
        "source = \"Vec2Position\"\n"
        "mapping = \"LiftXY_XY0\"\n"
        "orientation = \"World\"\n"
        "scale = \"World\"\n"
        "projection = \"OrthoViewport\"\n"
        "\n"
        "[[textures]]\n"
        "name = \"base_color\"\n"
        "asset = \"ABeautifulGame.Chessboard_base.Tex2D\"\n"
        "\n"
        "[[textures]]\n"
        "name = \"normal\"\n"
        "asset = \"ABeautifulGame.Chessboard_normal.Tex2D\"\n"
        "\n"
        "[[textures]]\n"
        "name = \"orm\"\n"
        "asset = \"texfile:direct/from/bytes.Tex2D\"\n";

    bool TestRecipeDependencies()
    {
        AssetLoadGraph graph;
        int edges = -1;

        if(!ExtractRecipeDependencies(kRecipeWithDeps, static_cast<int>(std::strlen(kRecipeWithDeps)),
                                      AnsiString("ABeautifulGame.StaticMesh/"),
                                      AnsiString("ABeautifulGame.StaticMesh/LoadModel.Chess.Chessboard.recipe.toml"),
                                      graph, &edges))
        {
            GLogError(u8"配方依赖提取失败");
            return false;
        }

        // 只有前两条是包内路径；`texfile:` 是运行期直接取字节的资源 ID
        if(edges != 2)
        {
            GLogError(u8"配方依赖边数不符，期望 2 实际 %d", edges);
            return false;
        }

        AssetDepKey root;
        NormalizeAssetRef(AnsiString("ABeautifulGame.StaticMesh/LoadModel.Chess.Chessboard.recipe.toml"),
                          asset::AssetType::MaterialRecipe, root);

        const AssetDepEdge *list = graph.GetDependencies(root);

        if(!list || std::strcmp(list[0].target.path,
                                "abeautifulgame.staticmesh/abeautifulgame.chessboard_base") != 0
                 || list[0].kind != AssetDepKind::Texture)
        {
            GLogError(u8"配方纹理依赖身份不符");
            return false;
        }

        // 闭包：配方 → 2 张贴图，且贴图排在配方之前
        AssetDepKey scene_root;
        NormalizeAssetRef(AnsiString("ABeautifulGame.StaticMesh/ABeautifulGame.model"),
                          asset::AssetType::Model, scene_root);

        AssetDepEdge scene_edge;
        scene_edge.target = root;
        scene_edge.kind   = AssetDepKind::MaterialRecipe;
        graph.SetDependencies(scene_root, &scene_edge, 1);

        ValueArray<AssetDepKey> closure;

        if(!graph.SolveClosure(scene_root, closure) || closure.GetCount() != 3)
        {
            GLogError(u8"场景 → 配方 → 纹理 的闭包数量不符，实际 %d", closure.GetCount());
            return false;
        }

        for(int i = 0; i < closure.GetCount(); i++)
            if(closure[i].MakeId() == root.MakeId())
            {
                if(i != closure.GetCount() - 1)
                {
                    GLogError(u8"拓扑序错误：配方应排在它依赖的纹理之后");
                    return false;
                }
            }

        return true;
    }
}

int main()
{
    hgl::logger::InitLogger(OS_TEXT("TestModelDependencyExtractor"));

    if(!TestLegacyModelDependencies()
       || !TestEmbeddedGeometryIsNotADependency()
       || !TestBrokenPackRejected()
       || !TestRecipeDependencies())
    {
        GLogError(u8"=== TestModelDependencyExtractor: 测试失败 ===");
        return 1;
    }

    GLogInfo(u8"=== TestModelDependencyExtractor: 全部测试通过 ===");
    return 0;
}
