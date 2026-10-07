// 图标墙：读 res/image/icon/freepik/list.txt 拿到 50 个图标名，按 1 秒 1 个的节奏
// 逐个加载进一张 50 层 Texture2DArray，并在屏幕上按 10×5 排列显示。
//
// 一次演示三件事：
//   ① 流式加载：启动即可出帧/交互，图标随秒逐个补上（不是开局一次性读 50 张）；
//   ② ChangeTexture2DArray：每张图标写进数组纹理自己那一层（层号 = list.txt 顺序）；
//   ③ 全批渲染：50 个格子共用同一几何 + 同一材质配方 ⇒ 同 program、同一个批。
//
// 屏幕空间由 Make2DNodeConfigZeroToOne 提供（0..1 铺满视口，位置映射 ZeroOneToNDC），
// 所以每格只需在实体变换里缩放/平移到格位，不需要相机。

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/filesystem/Filename.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/mtl/MaterialDefinitionRegistry.h>
#include<hgl/io/LoadStringList.h>
#include<hgl/utf.h>
#include<hgl/log/Log.h>

// ECS headers
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/support/TransformAccessor.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/components/MaterialData.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{
    constexpr uint32_t kGridCols  = 10;
    constexpr uint32_t kGridRows  = 5;
    constexpr uint32_t kIconCount = kGridCols * kGridRows;      // 50 = 墙的格数 = 数组层数

    constexpr uint32_t kIconSize     = 512;                     // 图标边长（资产实测）
    constexpr float    kLoadInterval = 1.0f;                    // 秒 / 个

    constexpr const os_char *kIconDirName = OS_TEXT("res/image/icon/freepik");
    constexpr const os_char *kIconListFile= OS_TEXT("res/image/icon/freepik/list.txt");

    constexpr float kCellW = 1.0f / float(kGridCols);           // 屏幕占比：1/10 宽
    constexpr float kCellH = 1.0f / float(kGridRows);           // 屏幕占比：1/5 高

    // 位置映射（实测标定，零假设）：
    //   Make2DNodeConfigZeroToOne 把顶点 0..1 映射成 NDC -1..1，随后实体变换以
    //   **屏幕中心为原点**叠加：t=0 ⇒ 该轴居中，+X 向右、+Y 向**下**（Y 轴上下翻转）。
    //   于是「第 c 列 / 第 r 行」的格心 NDC = (0.2c-0.9, 0.4r-0.8)。
    //   缩放取屏幕占比本身（0.1 ⇒ 占屏宽 1/10）。
    constexpr float CellPosX(const uint32_t col) { return 0.2f * float(col) - 0.9f; }
    constexpr float CellPosY(const uint32_t row) { return 0.4f * float(row) - 0.8f; }

    GeometryVertexFormat CreateIconQuadVertexFormat()
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V2F},
            {VertexSemantic::TexCoord, VF_V2F},
        };
        return gvf;
    }

    // 单位方形（0..1）：格位全靠实体变换缩放/平移（位置映射 = ZeroOneToNDC）
    constexpr float kQuadPosition[12] =
    {
        0,0,  0,1,  1,0,
        1,0,  0,1,  1,1
    };

    constexpr float kQuadTexCoord[12] =
    {
        0,0,  0,1,  1,0,
        1,0,  0,1,  1,1
    };
}

class TextureArrayStreamingApp:public WorkObject
{
private:

    ECSContext *        ecs_world  = nullptr;

    Texture2DArray *    icon_array = nullptr;
    Sampler *           sampler    = nullptr;
    graph::mtl::MaterialRecipe icon_recipe{};
    PrimitiveAsset      icon_asset{};

    U8StringList        icon_names;                 // list.txt 逐行（文件名，无扩展名）

    Entity *            icon_entity[kIconCount]{};  // 已上墙的格子（逐个创建）
    uint32_t            loaded_count = 0;
    double              load_timer   = 0.0;
    bool                load_aborted = false;

private:

    bool InitIconList()
    {
        const int count = LoadStringListFromTextFile(icon_names, kIconListFile);

        if (count <= 0)
        {
            GLogError(u8"[IconWall] list.txt 读取失败（count=%d）", count);
            return false;
        }

        if (count < int(kIconCount))
        {
            GLogError(u8"[IconWall] 列表只有 %d 项，墙需要 %u 项", count, kIconCount);
            return false;
        }

        GLogInfo(u8"[IconWall] list.txt 共 %d 项；墙 = %u 格（%u×%u），按 %.1f 秒/个流式加载",
                 count, kIconCount, kGridCols, kGridRows, kLoadInterval);
        return true;
    }

    bool InitTextureArray()
    {
        auto *tex_manager = GetManager<TextureManager>();
        if (!tex_manager)
            return false;

        // 一次性建满 50 层：后面逐个填的是"层的内容"，不是"层本身"
        // （带名字的重载取 AnsiString ⇒ 传窄串字面量，不是 OS_TEXT）
        icon_array = tex_manager->CreateTexture2DArray("IconWall.freepik",
                                                      kIconSize, kIconSize,
                                                      kIconCount,
                                                      PF_BC7UN,
                                                      1);        // 1 = 只用 0 级（资产自带链）
        if (!icon_array)
            return false;

        GLogInfo(u8"[IconWall] Texture2DArray 已建：%ux%u × %u 层 PF_BC7UN",
                 kIconSize, kIconSize, kIconCount);
        return true;
    }

    bool InitMaterial()
    {
        auto *sampler_manager = GetManager<SamplerManager>();
        if (!sampler_manager)
            return false;

        sampler = sampler_manager->CreateSampler();

        icon_recipe.recipe_name = "TextureArrayStreaming.UnlitTexture";
        icon_recipe.mtl_def_id  = "UnlitTexture";
        icon_recipe.render_state_overrides.pipeline_config = mtl::MakeSolid2DConfig();
        icon_recipe.vertex_node_config = graph::mtl::Make2DNodeConfigZeroToOne(true);

        return true;
    }

    bool InitQuadGeometry()
    {
        auto *device           = GetDevice();
        auto *buffer_manager   = GetManager<BufferManager>();
        auto *geometry_manager = GetManager<GeometryManager>();
        if (!device || !buffer_manager || !geometry_manager)
            return false;

        GeometryCreater pc(device, CreateIconQuadVertexFormat(), buffer_manager);
        pc.Init("IconQuad", 6);
        if (!pc.WriteVAB(VAN::Position, VF_V2F, kQuadPosition) ||
            !pc.WriteVAB(VAN::TexCoord, VF_V2F, kQuadTexCoord))
            return false;

        auto *geometry = pc.Create();
        if (!geometry)
            return false;

        geometry_manager->Add(geometry);
        icon_asset = PrimitiveAsset(geometry, &icon_recipe, PrimitiveType::Triangles);

        return true;
    }

    bool InitECS()
    {
        ecs_world = GetECSContext();
        return ecs_world != nullptr;
    }

    // 把第 index 个图标加载进数组纹理的第 index 层，并把它挂到墙上第 index 格
    bool AddIcon(const uint32_t index)
    {
        auto *tex_manager = GetManager<TextureManager>();
        if (!tex_manager || !ecs_world)
            return false;

        const U8String &icon_name = icon_names[index];

        const OSString filename = filesystem::JoinPathWithFilename(kIconDirName,
                                                                  ToOSString(icon_name) + OS_TEXT(".Tex2D"));

        // 层上传：引擎内部经 TextureManager::ChangeTexture2DArray(tex, buf, scope, layer, 1)
        // （源缓冲 = .Tex2D 的 0 级载荷；数组纹理按 layer 参数定位目标层）
        if (!tex_manager->LoadTexture2DArray(icon_array, index, filename))
        {
            GLogError(u8"[IconWall] 第 %u 层加载失败：%s", index, icon_name.c_str());
            return false;
        }

        const uint32_t col = index % kGridCols;
        const uint32_t row = index / kGridCols;

        Entity *entity = ecs_world->CreateEntity<Entity>();

        auto transform = ecs_world->GetTransform(
            ecs_world->CreateTransform(entity->GetEntityID(), Mobility::Static));

        // 格心：第 0 个图标落在**左上角**（Y 轴 + 向下 ⇒ 行号直接正向）
        transform.SetLocalPosition(glm::vec3(CellPosX(col), CellPosY(row), 0.0f));
        transform.SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform.SetLocalScale(glm::vec3(kCellW, kCellH, 1.0f));
        transform.SetMobility(Mobility::Static);

        ecs_world->GetOrCreateGeometryData(entity)->SetPrimitiveAsset(&icon_asset);

        if (!ecs_world->GetOrCreateMaterialData(entity)->SetTextureResource(
                "base_color",
                icon_array,
                sampler,
                MaterialTextureResourceKind::Texture2DArray,
                "",
                index))
            return false;

        icon_entity[index] = entity;
        ++loaded_count;

        GLogInfo(u8"[IconWall] %u/%u 已上墙：层=%u 格=(%u,%u) %s",
                 loaded_count, kIconCount, index, col, row, icon_name.c_str());
        return true;
    }

public:
    TextureArrayStreamingApp() = default;
    ~TextureArrayStreamingApp() override = default;

    bool Init() override
    {
        if (!InitIconList())
            return false;

        if (!InitTextureArray())
            return false;

        if (!InitMaterial())
            return false;

        if (!InitQuadGeometry())
            return false;

        if (!InitECS())
            return false;

        return true;
    }

    void Tick(double delta_time) override
    {
        WorkObject::Tick(delta_time);

        if (load_aborted || loaded_count >= kIconCount)
            return;

        load_timer += delta_time;

        if (load_timer < double(kLoadInterval))
            return;

        load_timer -= double(kLoadInterval);

        if (!AddIcon(loaded_count))
        {
            // 失败不重试：记一次即停，避免每秒刷屏掩盖首因
            GLogError(u8"[IconWall] 流式加载中止于第 %u 个", loaded_count);
            load_aborted = true;
            return;
        }

        if (loaded_count >= kIconCount)
            GLogInfo(u8"[IconWall] 全部 %u 个图标已上墙（%u×%u，单批绘制）",
                     kIconCount, kGridCols, kGridRows);
    }
};//class TextureArrayStreamingApp:public WorkObject

int os_main(int argc,os_char **argv)
{
    return RunFramework<TextureArrayStreamingApp>(
        OS_TEXT("Stream 50 freepik icons into a Texture2DArray (1 icon/sec, 10x5 grid)"),
        argc, argv, 1600, 800);
}
