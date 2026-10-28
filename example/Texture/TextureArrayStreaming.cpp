// 图标墙：读 res/image/icon/freepik/list.txt 拿到 50 个图标名，流式加载进一张 50 层
// Texture2DArray，并在屏幕上按 10×5 排列显示。
//
// 一次演示四件事：
//   ① 两段式异步加载：LoadedFileManager 在 IO 线程池里并发读 50 个 .Tex2D，
//      主线程只在 Tick 里 Pump() 得到回调 —— 不再像旧版那样每 0.1 秒同步读盘卡一帧；
//   ② 逐层上传 ChangeTexture2DArray：回调里用 TextureLoadCommitter 把内存字节写进
//      数组纹理自己那一层（层号 = list.txt 顺序）；
//   ③ 帧预算驱动的上墙节奏：Pump(max_dispatch) 决定每帧最多提交几个图标，
//      50 个图标在几帧内平滑补齐，而不是开局阻塞；
//   ④ 全批渲染：50 个格子共用同一几何 + 同一材质配方 ⇒ 同 program、同一个批。
//
// 屏幕空间由 Make2DNodeConfigZeroToOne 提供（0..1 铺满视口，位置映射 ZeroOneToNDC），
// 所以每格只需在实体变换里缩放/平移到格位，不需要相机。

#include<hgl/framework/WorkManager.h>
#include<hgl/framework/LoadedFileManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/filesystem/Filename.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/TextureLoadCommitter.h>
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

    constexpr const os_char *kIconDirName = OS_TEXT("res/image/icon/freepik");
    constexpr const os_char *kIconListFile= OS_TEXT("res/image/icon/freepik/list.txt");

    constexpr int      kPumpPerFrame = 2;                       // 每帧最多提交多少个图标

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

class TextureArrayStreamingApp;

/**
 * 单个图标的加载接收者：业务上下文(层号、图标名)就是成员。
 *
 * 回调只在 Tick() 里 Pump() 的线程触发，因此可以直接创建 ECS 实体、
 * 直接调用 ChangeTexture2DArray 这类主线程接口。
 */
class IconLoadHandler:public LoadedFileHandler
{
public:

    TextureArrayStreamingApp *  app=nullptr;
    uint32                      layer=0;                //!< 目标数组层 = 墙上格位序号
    U8String                    icon_name;              //!< 仅用于日志

    bool                        loaded=false;
    bool                        failed=false;

public:

    void OnFileLoaded   (const FileLoadTicket &ticket,LoadedFile *file)override;
    void OnFileFailed   (const FileLoadTicket &ticket,io::AsyncFileLoadError error)override;
    void OnFileCancelled(const FileLoadTicket &ticket)override;
};

class TextureArrayStreamingApp:public WorkObject
{
public:

    /** 回调里用：把一层图标挂到墙上 */
    bool AttachIcon(const uint32_t layer,const U8String &icon_name);

private:

    ECSContext *        ecs_world  = nullptr;

    Texture2DArray *    icon_array = nullptr;
    Sampler *           sampler    = nullptr;
    graph::mtl::MaterialRecipe icon_recipe{};
    PrimitiveAsset      icon_asset{};

    U8StringList        icon_names;                 // list.txt 逐行（文件名，无扩展名）

    LoadedFileManager       load_manager;           // 第一段：后台并发读盘
    TextureLoadCommitter    committer;              // 第二段：主线程写层

    IconLoadHandler     handlers[kIconCount];       // 每个图标一个接收者，必须活到回调结束
    bool                request_sent[kIconCount]{}; // 是否已发出读盘请求

    uint32_t            submitted_count = 0;
    uint32_t            loaded_count    = 0;
    uint32_t            failed_count    = 0;
    bool                finished        = false;
    bool                reported        = false;

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

        GLogInfo(u8"[IconWall] list.txt 共 %d 项；墙 = %u 格（%u×%u），每帧最多提交 %d 个",
                 count, kIconCount, kGridCols, kGridRows, kPumpPerFrame);
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

    /**
     * 第一段：把还没提交的图标请求推进读盘队列。
     * @param budget 本次最多提交几个，0=不限
     * @return 本次提交的数量
     */
    int SubmitMore(uint32 budget)
    {
        int submitted_now=0;

        while(submitted_count<kIconCount&&(budget==0||submitted_now<budget))
        {
            const uint32 index=submitted_count;

            if(!request_sent[index])
            {
                const OSString filename=filesystem::JoinPathWithFilename(kIconDirName,
                                                                        ToOSString(icon_names[index])+OS_TEXT(".Tex2D"));

                IconLoadHandler &handler=handlers[index];

                handler.app      = this;
                handler.layer    = index;
                handler.icon_name=icon_names[index];

                // 优先级按层号递减：屏幕上靠前的格子先出图
                const io::AsyncFilePriority priority = (index<kGridCols)      ? io::AsyncFilePriority::High
                                                     : (index<kGridCols*2)    ? io::AsyncFilePriority::Normal
                                                                              : io::AsyncFilePriority::Low;

                if(load_manager.Submit(filename,&handler,priority)==0)
                {
                    GLogError(u8"[IconWall] 第 %u 个图标提交失败", index);
                    ++failed_count;
                    request_sent[index]=true;           // 不再重试，避免每帧刷屏
                    ++submitted_count;
                    continue;
                }

                request_sent[index]=true;
                ++submitted_count;
                ++submitted_now;
            }
            else
                ++submitted_count;                      // 已在途

            if(submitted_count>=kIconCount)
                break;
        }

        return submitted_now;
    }

public:

    TextureArrayStreamingApp() = default;

    ~TextureArrayStreamingApp() override
    {
        load_manager.Stop(thread::ShutdownMode::Drain);
    }

    /** 第二段：把一个已读出的图标写进指定层并挂到墙上(在 Pump 的线程上执行) */
    bool CommitIconLayer(LoadedFile *file,const uint32_t layer,const U8String &icon_name)
    {
        if(!file||!icon_array)
            return false;

        if(!committer.CommitTexture2DArrayLayer(file,icon_array,layer))
        {
            GLogError(u8"[IconWall] 第 %u 层写入失败：%s", layer, icon_name.c_str());
            return false;
        }

        return AttachIcon(layer,icon_name);
    }

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

        auto *tex_manager=GetManager<TextureManager>();
        if (!tex_manager)
            return false;

        committer.SetTextureManager(tex_manager);

        if (!load_manager.Start(2))
        {
            GLogError(u8"[IconWall] IO 线程池启动失败");
            return false;
        }

        // 开局只提交第一批，其余的由 Tick 按帧预算补投
        SubmitMore(kPumpPerFrame*4);

        return true;
    }

    void Tick(double delta_time) override
    {
        WorkObject::Tick(delta_time);

        if(finished)
            return;

        SubmitMore(kPumpPerFrame);                          // 补投读盘请求
        load_manager.Pump(kPumpPerFrame);                   // 第二段：限量提交上墙
        load_manager.Collect();                             // 帧末回收已放手的文件

        if(!reported&&loaded_count+failed_count>=kIconCount)
        {
            reported  = true;
            finished  = true;

            const LoadedFileManagerStatistics stats=load_manager.GetStatistics();

            GLogInfo(u8"[IconWall] 全部完成：成功 %u / 失败 %u（%u×%u，单批绘制）",
                     loaded_count, failed_count, kGridCols, kGridRows);
            GLogInfo(u8"[IconWall] 统计：submitted=%llu cache_hits=%llu loaded=%llu failed=%llu "
                        "alive_files=%llu pending_release=%llu released=%llu bytes=%llu",
                     stats.submitted, stats.cache_hits,
                     stats.dispatched_loaded, stats.dispatched_failed,
                     stats.alive_files, stats.pending_release, stats.released_files,
                     stats.io.loaded_bytes);
        }
    }

    void OnIconFailed(const uint32_t layer,const U8String &icon_name,io::AsyncFileLoadError error)
    {
        ++failed_count;

        GLogError(u8"[IconWall] 第 %u 层读取失败（err=%d）：%s",
                  layer, static_cast<int>(error), icon_name.c_str());
    }
};//class TextureArrayStreamingApp:public WorkObject

// ---------------------------------------------------------------- IconLoadHandler 实现
// 这些回调只在 Tick() 里 Pump() 的线程触发，因此可以直接做 ECS / 纹理层写入。

void IconLoadHandler::OnFileLoaded(const FileLoadTicket &,LoadedFile *file)
{
    loaded=true;

    if(!app)
        return;

    app->CommitIconLayer(file,layer,icon_name);
}

void IconLoadHandler::OnFileFailed(const FileLoadTicket &,io::AsyncFileLoadError error)
{
    failed=true;

    if(app)
        app->OnIconFailed(layer,icon_name,error);
}

void IconLoadHandler::OnFileCancelled(const FileLoadTicket &)
{
    if(app)
        app->OnIconFailed(layer,icon_name,io::AsyncFileLoadError::None);
}

// ---------------------------------------------------------------- 挂墙
bool TextureArrayStreamingApp::AttachIcon(const uint32_t index,const U8String &icon_name)
{
    if(!ecs_world)
        return false;

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

    ++loaded_count;

    GLogInfo(u8"[IconWall] %u/%u 已上墙：层=%u 格=(%u,%u) %s",
             loaded_count, kIconCount, index, col, row, icon_name.c_str());
    return true;
}

int os_main(int argc,os_char **argv)
{
    return RunFramework<TextureArrayStreamingApp>(
        OS_TEXT("Stream 50 freepik icons into a Texture2DArray (async, 10x5 grid)"),
        argc, argv, 1600, 800);
}