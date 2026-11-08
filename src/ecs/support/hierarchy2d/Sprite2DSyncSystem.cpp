#include<hgl/ecs/support/hierarchy2d/Sprite2DSyncSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/entity/Entity.h>
#include<hgl/ecs/components/Sprite2DComponent.h>
#include<hgl/ecs/components/Transform2DComponent.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/components/MaterialData.h>
#include<hgl/ecs/support/transform/TransformAccessor.h>
#include<hgl/ecs/systems/tick/Transform2DSystem.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/log/Log.h>
#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>

namespace hgl::ecs
{
    namespace
    {
        // 单位矩形 [0,1]x[0,1]（6 顶点三角形列表，与 TextureRect 示例同型）：
        // 配方的 mapping=ZeroOneToNDC 把它映到整个 NDC，TRS 再缩放到精灵的像素包围盒。
        constexpr float kRectPosition[12] =
        {
            0,0,
            0,1,
            1,0,
            1,0,
            0,1,
            1,1
        };

        constexpr float kRectTexCoord[12] =
        {
            0,0,
            0,1,
            1,0,
            1,0,
            0,1,
            1,1
        };

        graph::GeometryVertexFormat MakeRectVertexFormat()
        {
            graph::GeometryVertexFormat gvf{
                {graph::VertexSemantic::Position, VF_V2F},
                {graph::VertexSemantic::TexCoord, VF_V2F},
            };
            return gvf;
        }
    }

    Sprite2DSyncSystem::Sprite2DSyncSystem(const AnsiString& name)
        : System(name)
    {
        // 2D 层级求值之后、渲染收集之前：写出的 3D TRS 要赶在 L2W 上传前生效
        SetExecutionPhase(ExecutionPhase::TickPostCamera);
        AddDependency<Transform2DSystem>();
    }

    bool Sprite2DSyncSystem::EnsureResources()
    {
        if (resources_ready)
            return true;
        if (!world)
            return false;

        auto *graphics = world->GetGraphicsContext();
        if (!graphics)
            return false;

        auto *device          = graphics->GetDevice();
        auto *buffer_manager  = graphics->GetManager<graph::BufferManager>();
        auto *geometry_manager = graphics->GetManager<graph::GeometryManager>();
        if (!device || !buffer_manager || !geometry_manager)
            return false;

        const auto *recipe = graphics->FindOrLoadMaterialRecipe("Sprite2D.UnlitTexture");
        if (!recipe)
            return false;

        graph::GeometryCreater gc(device, MakeRectVertexFormat(), buffer_manager);
        if (!gc.Init("Sprite2DRect", 6) ||
            !gc.WriteVAB(graph::VAN::Position, VF_V2F, kRectPosition) ||
            !gc.WriteVAB(graph::VAN::TexCoord, VF_V2F, kRectTexCoord))
        {
            GLogWarning(u8"[Sprite2DSyncSystem] 单位矩形几何创建失败");
            return false;
        }

        auto *geometry = gc.Create();
        if (!geometry)
            return false;
        geometry_manager->Add(geometry);

        // PrimitiveAsset 只存配方指针 ⇒ 快照进成员，保证指针比注册表条目更长寿
        recipe_copy = *recipe;
        rect_asset = graph::PrimitiveAsset(geometry, &recipe_copy, graph::PrimitiveType::Triangles);

        resources_ready = true;
        return true;
    }

    void Sprite2DSyncSystem::Update(float /*deltaTime*/)
    {
        if (!world)
            return;

        if (!EnsureResources())
        {
            if (!warned_no_resources)
            {
                warned_no_resources = true;
                GLogWarning(u8"[Sprite2DSyncSystem] 渲染资源未就绪（GraphicsContext/配方缺失？），精灵同步跳过");
            }
            return;
        }

        auto *rt = world->GetRenderTarget();
        if (!rt)
            return;

        const auto extent = rt->GetExtent();
        const float vw = static_cast<float>(extent.width);
        const float vh = static_cast<float>(extent.height);
        if (vw <= 0.0f || vh <= 0.0f)
            return;

        ValueArray<Sprite2DComponent*> sprites;
        world->GetComponents<Sprite2DComponent>(sprites);

        for (auto *sprite : sprites)
        {
            if (!sprite)
                continue;

            auto *entity = sprite->GetOwner();
            if (!entity)
                continue;

            // -- 2D 层级节点：缺则补挂（按根节点处理；作者设置位置/尺寸前不会出图） --
            auto *t2 = entity->GetComponent<Transform2DComponent>();
            if (!t2)
                t2 = entity->AddComponent<Transform2DComponent>();

            // -- 3D 载体 Transform（Movable：每帧由本系统重写 TRS） --
            const auto eid = entity->GetEntityID();
            auto tid = world->GetTransformID(eid);
            if (tid == INVALID_TRANSFORM_ID)
                tid = world->CreateTransform(eid, Mobility::Movable);
            if (tid == INVALID_TRANSFORM_ID)
                continue;

            const auto accessor = world->GetTransform(tid);

            // -- 几何/材质载体 --
            auto *geometry_data = world->GetOrCreateGeometryData(entity);
            if (geometry_data->GetPrimitiveAsset() != &rect_asset)
                geometry_data->SetPrimitiveAsset(&rect_asset);

            auto *material_data = world->GetOrCreateMaterialData(entity);

            // -- 像素 AABB → NDC TRS（像素空间原点左下 +Y 上，与 ZeroOneToNDC 同向，无需翻转） --
            const auto &b = t2->world_bounds;
            if (b.Width > 0.0f && b.Height > 0.0f)
            {
                const float cx = (b.GetLeft() + b.GetRight()) * 0.5f;
                const float cy = (b.GetBottom() + b.GetTop()) * 0.5f;

                accessor.SetLocalTRS(
                    glm::vec3(cx * 2.0f / vw - 1.0f, cy * 2.0f / vh - 1.0f, 0.0f),
                    glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                    glm::vec3(b.Width * 2.0f / vw, b.Height * 2.0f / vh, 1.0f));
            }

            // -- 纹理覆盖：仅在授权代变化时下发一次 --
            if (sprite->texture_generation != sprite->synced_generation)
            {
                sprite->synced_generation = sprite->texture_generation;

                // 若声明了 URI 但尚未解析句柄：向 TextureManager 索取/创建（MetaOnly 句柄自动进入流送）
                if (!sprite->texture_uri.IsEmpty() && !sprite->texture_asset && !sprite->texture)
                {
                    if (auto *gc = world->GetGraphicsContext())
                        if (auto *tex_manager = gc->GetTextureManager())
                            sprite->texture_asset = tex_manager->CreateTextureAsset(sprite->texture_uri, true);
                }

                if (sprite->texture)
                {
                    material_data->SetTextureResource("base_color", sprite->texture,
                                                      MaterialTextureResourceKind::Texture2D, 0, /*required=*/true);
                }
                else if (sprite->texture_asset)
                {
                    material_data->SetTextureResource("base_color", sprite->texture_asset,
                                                      MaterialTextureResourceKind::Texture2D, 0, /*required=*/true);
                }
            }
        }
    }
}//namespace hgl::ecs