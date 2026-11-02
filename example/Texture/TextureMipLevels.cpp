// mipmap 级别测试范例（2D 屏幕空间"级数尺"）：
//
//   A 行：普通 Texture2D（未压缩 RGBA8，9 级，链来自自产资产 MipLevels.RGBA8.Tex2D）
//   B 行：Texture2DArray（1 层 x 9 级，同一文件，链由 LoadTexture2DArray 逐层整链拷入
//        → 这一行是"数组链修复"的回归验证：修之前它只会显示 0 级）
//   C 行：Texture2DArray（1 层 x 7 级，源为自产 BC7 资产 MipLevels.BC7.Tex2D）
//        → 压缩格式 + 数组链；该资产每级内容不同（BC7 最小块 4x4，故到 4x4 为止），
//          预期与 A/B 行一样能看到逐级不同的颜色
//
// 每行是一把"级数尺"：第 L 个方块取 S = 256 >> L 像素的**正方形**，UV 恒为 0..1，
// 于是它在屏幕上把 256x256 的纹理正好压成 S x S → 两个方向的 texel/px 都是 256/S，
// 各向异性过滤取到的隐式 LOD = log2(256/S) = L。即"第 L 个方块必然采样第 L 级"。
// 方块必须是正方形：只压一个方向时，另一个方向的导数更大，LOD 会被抬高
// （实测 256px 宽 x 123px 高的矩形采样到的是 1 级而不是 0 级）。
//
// 资产里每一级的颜色/级别号都不同（0 红 1 橙 2 黄 3 绿 4 青 5 蓝 6 紫 7 品红 8 白），
// 所以画面直接读出"当前采样到第几级"；若 mip 链缺失（例如数组链断掉），
// 所有方块都会显示 0 级（红色带 "0" 的棋盘格），一眼可辨。
//
// 资产与生成脚本：res/image/mipmaps/ + scripts/gen_mipmap_level_assets.py

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/asset/TextureAsset.h>
#include<hgl/mtl/MaterialResolver.h>
#include<hgl/graph/ubo/ViewportInfo.h>
#include<hgl/math/Vector.h>
#include<hgl/log/Log.h>

#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/entity/Entity.h>
#include<hgl/ecs/support/transform/TransformAccessor.h>
#include<hgl/ecs/components/GeometryData.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<vector>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{
    constexpr uint32_t WINDOW_WIDTH  = 1280;
    constexpr uint32_t WINDOW_HEIGHT = 900;

    constexpr uint32_t MIP_SOURCE_SIZE = 256;       ///< 测试资产 0 级尺寸（256x256）

    constexpr uint32_t ROW_LEVELS[3] = { 9, 9, 7 }; ///< A/B 行 RGBA8 到 1x1，C 行 BC7 到 4x4
    constexpr float ROW_BAND[3]      = { 0.333f, 0.666f, 0.999f };  ///< 每行方块的底边（0..1 屏幕空间）

    constexpr float ROW_X_START   = 0.01f;
    constexpr float ROW_GAP_PIXEL = 6.0f;           ///< 方块之间的水平间隙（像素）


    GeometryVertexFormat CreateRectGeometryVertexFormat()
    {
        // 2D 屏幕空间矩形：只要 Position(V2) + TexCoord(V2)，不需要法线（UnlitTexture 的顶点需求）
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V2F},
            {VertexSemantic::TexCoord, VF_V2F},
        };
        return gvf;
    }

    /**
     * 生成一行"级数尺"的顶点数据。
     * 第 L 个方块 = S x S 像素（S = 256>>L），UV = 0..1，底边贴在 band_y 上 → 隐式 LOD 恰为 L。
     *
     * 尺寸必须按**实际 framebuffer 尺寸**换算（不是窗口逻辑尺寸）：2D 的 0..1 空间映射到 viewport，
     * DPI 缩放时 framebuffer 比逻辑尺寸大，按逻辑尺寸算会让每格都偏大、整条尺子偏移一级
     * （实测 1280 逻辑宽 / 1920 framebuffer 时，第 1 格报的是 0 级而不是 1 级）。
     */
    void BuildLevelRuler(const uint32_t levels, const float band_y,
                         const uint32_t viewport_width, const uint32_t viewport_height,
                         std::vector<float> &position, std::vector<float> &tex_coord)
    {
        position.clear();
        tex_coord.clear();

        const float gap = ROW_GAP_PIXEL / float(viewport_width);

        float x = ROW_X_START;

        for (uint32_t level = 0; level < levels; level++)
        {
            const uint32_t s = MIP_SOURCE_SIZE >> level;

            const float w = float(s) / float(viewport_width);
            const float h = float(s) / float(viewport_height);

            const float x0 = x;
            const float x1 = x + w;
            const float y0 = band_y - h;        ///< 底边对齐 band_y
            const float y1 = band_y;

            // 与 TextureRectArray 相同的绕序/UV 对应：先两个三角形
            const float px[12] = { x0, y0,  x0, y1,  x1, y0,   x1, y0,  x0, y1,  x1, y1 };
            const float uv[12] = { 0.0f, 0.0f,  0.0f, 1.0f,  1.0f, 0.0f,   1.0f, 0.0f,  0.0f, 1.0f,  1.0f, 1.0f };

            position.insert(position.end(), px, px + 12);
            tex_coord.insert(tex_coord.end(), uv, uv + 12);

            x = x1 + gap;
        }
    }
}//namespace

class TextureMipLevelsApp:public WorkObject
{
private:

    ECSContext *        ecs_world                = nullptr;

    /// 每行一个配方：三行是**三张不同纹理**，纹理身份是配方的固有属性
    /// （层清单在 `ShaderLibrary/asset/texarray/mip_levels_*.texarray.toml` 里）。
    graph::mtl::MaterialRecipe  ruler_recipe[3];
    PrimitiveAsset              ruler_asset[3]{};

    Entity *            row_entities[3]{};
    hgl::ecs::GeometryData *row_primitives[3]{};

    uint32_t            ruler_viewport_width  = 0;      ///< 当前级数尺是按这个尺寸建的（0 = 还没建）
    uint32_t            ruler_viewport_height = 0;
    bool                mip_spec_reported     = false;  ///< A 行走异步流式，等三行都就绪后再核对规格

private:

    bool InitMaterial()
    {
        // 三行 = 三份配方：同一份 UnlitTexture 契约，只有 `asset` 覆盖不同
        // （B/C 行是父配方的子配方，只写差异）。
        static const char *row_recipe_name[3] =
        {
            "TextureMipLevels.UnlitTexture",    // A 行：RGBA8 2D，9 级
            "TextureMipLevels.ArrayRgba8",      // B 行：RGBA8 单层数组，9 级
            "TextureMipLevels.ArrayBc7",        // C 行：BC7 单层数组，7 级
        };

        for (uint32_t row = 0; row < 3; row++)
        {
            const mtl::MaterialRecipe *loaded_recipe = GetRecipe(row_recipe_name[row]);

            if (!loaded_recipe)
            {
                GLogError("InitMaterial: GetRecipe failed (row=%u, name=%s)", row, row_recipe_name[row]);
                return false;
            }

            // UnlitTexture：2D 模式专用，只采样 base_color，无光照干扰 → 屏幕上就是资产本身的颜色
            ruler_recipe[row] = *loaded_recipe;
        }

        return true;
    }

    /// 三行纹理都就绪后核对一次 mip 规格。A 行走异步流式装配，Init 时还拿不到纹理，
    /// 所以核对只能放到第一帧之后做（这也是本示例的存在意义：确认三条链真的不同）。
    void ReportMipSpecIfReady()
    {
        if (mip_spec_reported)
            return;

        Texture *row_texture[3]{};

        for (uint32_t row = 0; row < 3; row++)
        {
            for (const mtl::RecipeTextureBinding &binding : ruler_recipe[row].textures)
                if (binding.texture_name == "base_color")
                    row_texture[row] = binding.asset ? binding.asset->GetTexture() : nullptr;

            if (!row_texture[row])
                return;
        }

        mip_spec_reported = true;

        auto *texture_2d    = static_cast<Texture2D *>     (row_texture[0]);
        auto *array_rgba8   = static_cast<Texture2DArray *>(row_texture[1]);
        auto *array_bc7     = static_cast<Texture2DArray *>(row_texture[2]);

        GLogInfo("[MipTest] A 2D       %ux%u fmt=%u mip_levels=%u  (MipLevels.RGBA8.Tex2D)",
                 texture_2d->GetWidth(), texture_2d->GetHeight(),
                 uint32_t(texture_2d->GetFormat()), texture_2d->GetMipLevel());

        GLogInfo("[MipTest] B 2DArray  %ux%u layers=%u fmt=%u mip_levels=%u",
                 array_rgba8->GetWidth(), array_rgba8->GetHeight(),
                 array_rgba8->GetLayer(),
                 uint32_t(array_rgba8->GetFormat()), array_rgba8->GetMipLevel());

        GLogInfo("[MipTest] C 2DArray  %ux%u layers=%u fmt=%u mip_levels=%u  (MipLevels.BC7.Tex2D, BC7)",
                 array_bc7->GetWidth(), array_bc7->GetHeight(),
                 array_bc7->GetLayer(),
                 uint32_t(array_bc7->GetFormat()), array_bc7->GetMipLevel());

        if (texture_2d->GetMipLevel() != 9 ||
            array_rgba8->GetMipLevel() != 9 ||
            array_bc7->GetMipLevel() != 7)
        {
            GLogError("ReportMipSpec: mip level count unexpected (2d=%u array=%u bc7array=%u)",
                      texture_2d->GetMipLevel(),
                      array_rgba8->GetMipLevel(),
                      array_bc7->GetMipLevel());
        }
    }

    bool InitRow(const uint32_t row, const uint32_t viewport_width, const uint32_t viewport_height)
    {
        auto *device = GetDevice();
        auto *buffer_manager = GetManager<BufferManager>();
        auto *geometry_manager = GetManager<GeometryManager>();

        if (!device || !buffer_manager || !geometry_manager)
            return false;

        std::vector<float> position;
        std::vector<float> tex_coord;

        BuildLevelRuler(ROW_LEVELS[row], ROW_BAND[row], viewport_width, viewport_height, position, tex_coord);

        GeometryCreater pc(device, CreateRectGeometryVertexFormat(), buffer_manager);
        pc.Init("MipLevelRuler", uint32_t(position.size() / 2));

        if (!pc.WriteVAB(VAN::Position, VF_V2F, position.data()) ||
            !pc.WriteVAB(VAN::TexCoord, VF_V2F, tex_coord.data()))
        {
            GLogError("InitRow: WriteVAB failed (row=%u)", row);
            return false;
        }

        auto *geometry = pc.Create();

        if (!geometry)
        {
            GLogError("InitRow: Create geometry failed (row=%u)", row);
            return false;
        }

        geometry_manager->Add(geometry);

        ruler_asset[row] = PrimitiveAsset(geometry, &ruler_recipe[row], PrimitiveType::Triangles);

        return true;
    }

    bool InitECS()
    {
        ecs_world = GetECSContext();

        if (!ecs_world)
            return false;

        for (uint32_t row = 0; row < 3; row++)
        {
            row_entities[row] = ecs_world->CreateEntity<Entity>("MipLevelRow_" + std::to_string(row));

            auto transform = ecs_world->GetTransform(ecs_world->CreateTransform(row_entities[row]->GetEntityID(), Mobility::Static));
            transform.SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
            transform.SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            transform.SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
            transform.SetMobility(Mobility::Static);

            auto primitive = row_entities[row]->GetContext()->GetOrCreateGeometryData(row_entities[row]);
            // 材质数据层组件（配方/绑定/行号的承载者）——渲染侧需要它存在
            row_entities[row]->GetContext()->GetOrCreateMaterialData(row_entities[row]);
            primitive->GetOwner()->GetContext()->GetOrCreateGeometryData(primitive->GetOwnerID())->SetPrimitiveAsset(&ruler_asset[row]);

            row_primitives[row] = primitive;

            // 纹理已由配方声明（ruler_recipe[row].AddTexture），实体端不再重复绑定
            // [A5a] 可见性真值已收敛到实体级（默认即可见）：原组件级 SetVisible(true) 等义调用已删
        }

        GLogInfo("[MipTest] rows=3 : A=2D(RGBA8,9级) B=2DArray(RGBA8,9级) C=2DArray(BC7,7级)；"
                 "每行第 L 个方块 = %u>>L 像素见方 → 必然采样第 L 级；A/B 两行应逐格同色",
                 MIP_SOURCE_SIZE);

        return true;
    }

    bool InitRuler()
    {
        // 首帧前先用窗口逻辑尺寸建一版；Tick 里拿到真实 viewport 尺寸后会重建（见 RebuildRulerIfNeeded）
        const VkExtent2D *extent = GetExtent();

        const uint32_t vw = (extent && extent->width ) ? extent->width  : WINDOW_WIDTH;
        const uint32_t vh = (extent && extent->height) ? extent->height : WINDOW_HEIGHT;

        for (uint32_t row = 0; row < 3; row++)
            if (!InitRow(row, vw, vh))
                return false;

        ruler_viewport_width  = vw;
        ruler_viewport_height = vh;

        GLogInfo("[MipTest] 级数尺按 %ux%u 建（窗口逻辑 %ux%u）", vw, vh, WINDOW_WIDTH, WINDOW_HEIGHT);

        return true;
    }

    /**
     * 视口尺寸与建尺子时不一致时重建。
     *
     * 必须用**真实 viewport 尺寸**（viewport UBO 的 viewport_resolution，每帧更新）：
     * DPI 缩放时它比窗口逻辑尺寸大（实测 2 倍），按逻辑尺寸算会让每格偏大一倍、
     * 整条尺子偏移一级（第 1 格报 0 级而不是 1 级）。
     */
    void RebuildRulerIfNeeded()
    {
        const graph::ViewportInfo *vp = GetViewportInfo();

        if (!vp)
            return;

        const uint32_t vw = vp->GetViewportWidth();
        const uint32_t vh = vp->GetViewportHeight();

        if (vw == 0 || vh == 0)
            return;

        if (vw == ruler_viewport_width && vh == ruler_viewport_height)
            return;

        for (uint32_t row = 0; row < 3; row++)
        {
            if (!InitRow(row, vw, vh))
                return;

            if (row_primitives[row])
                row_primitives[row]->GetOwner()->GetContext()->GetOrCreateGeometryData(row_primitives[row]->GetOwnerID())->SetPrimitiveAsset(&ruler_asset[row]);
        }

        ruler_viewport_width  = vw;
        ruler_viewport_height = vh;

        GLogInfo("[MipTest] 级数尺重建：真实 viewport = %ux%u（窗口逻辑 %ux%u）", vw, vh, WINDOW_WIDTH, WINDOW_HEIGHT);
    }

public:

    TextureMipLevelsApp() = default;

    void Tick(double) override
    {
        ReportMipSpecIfReady();
        RebuildRulerIfNeeded();
    }

    ~TextureMipLevelsApp()
    {
        // 所有权单点：纹理由 TextureManager 持有、句柄归 TextureManager 的句柄表、
        // ecs_world 由框架持有。示例只做观察，析构不得 delete / SAFE_CLEAR ——
        // 否则管理器收尾时会二次 delete（实测 AV @ 0xFEEEFEEE）
        ecs_world = nullptr;
    }

    bool Init() override
    {
        if (!InitMaterial())
            return false;

        if (!InitRuler())
            return false;

        if (!InitECS())
            return false;

        SetClearColor(Color4f(0.02f, 0.02f, 0.03f, 1.0f));

        return true;
    }
};//class TextureMipLevelsApp:public WorkObject

int os_main(int argc, os_char **argv)
{
    return RunFramework<TextureMipLevelsApp>(OS_TEXT("Mipmap Level Chain Test (2D RGBA8 / 2DArray RGBA8 / 2DArray BC7)"), argc, argv, WINDOW_WIDTH, WINDOW_HEIGHT);
}
