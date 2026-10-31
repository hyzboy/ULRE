#include<hgl/framework/WorkObject.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/ecs/systems/tick/InputSystem.h>
#include<hgl/graph/module/SwapchainModule.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/asset/GeometryAssetRegistry.h>
#include<hgl/graph/asset/GeometryAsset.h>
#include<hgl/graph/asset/MaterialDataBlockRegistry.h>
#include<hgl/vk/VKRenderTargetSwapchain.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/time/Time.h>
#include<hgl/utf.h>
#include<hgl/log/Log.h>
//#include<iostream>

namespace hgl
{
    void WorkObject::SetECSContext(ecs::ECSContext *ctx)
    {
        world = ctx;
    }

    const graph::mtl::MaterialRecipe *WorkObject::GetRecipe(const char *name)
    {
        auto *graphics = GetGraphicsContext();
        if (!graphics)
        {
            GLogError(u8"[WorkObject] 无法查找材质配方：GraphicsContext 未就绪 recipe=%s",
                      name ? name : "<null>");
            return nullptr;
        }
        return graphics->FindOrLoadMaterialRecipe(name);
    }

    const graph::GeometryAsset *WorkObject::GetGeometryAsset(const char *uri,
                                                             const graph::GeometryVertexFormat *format_override)
    {
        auto *graphics = GetGraphicsContext();
        if (!graphics)
        {
            GLogError(u8"[WorkObject] 无法解析几何 URI：GraphicsContext 未就绪 uri=%s",
                      uri ? uri : "<null>");
            return nullptr;
        }

        if (!uri || !uri[0])
        {
            GLogError(u8"[WorkObject] 无法解析几何 URI：URI 为空");
            return nullptr;
        }

        return graphics->GetGeometryAssetRegistry()->GetOrLoadURI(ToOSString(uri), format_override);
    }

    graph::Geometry *WorkObject::GetGeometry(const char *uri,
                                             const graph::GeometryVertexFormat *format_override)
    {
        const graph::GeometryAsset *asset = GetGeometryAsset(uri, format_override);

        return asset ? asset->GetGeometry() : nullptr;
    }

    graph::GeometryAssetRegistry *WorkObject::GetGeometryAssetRegistry()const
    {
        auto *graphics = GetGraphicsContext();
        return graphics ? graphics->GetGeometryAssetRegistry() : nullptr;
    }

    bool WorkObject::GetMaterialDataBinding(const char *uri, const uint32_t row_index,
                                            graph::GlobalSSBOBinding &out)
    {
        if (!uri || !uri[0])
        {
            GLogError(u8"[WorkObject] 无法解析数据块 URI：URI 为空");
            return false;
        }

        auto *graphics = GetGraphicsContext();
        if (!graphics)
        {
            GLogError(u8"[WorkObject] 无法解析数据块 URI：GraphicsContext 未就绪 uri=%s", uri);
            return false;
        }

        auto *blocks = graphics->GetMaterialDataBlockRegistry();
        if (!blocks)
        {
            GLogError(u8"[WorkObject] 无法解析数据块 URI：MaterialDataBlockRegistry 未就绪 uri=%s", uri);
            return false;
        }

        return blocks->GetRowBinding(GetManager<graph::GlobalSSBOBufferRegistry>(),
                                     ToOSString(uri), row_index, out);
    }

    bool WorkObject::LoadRecipeFile(const OSString &path)
    {
        auto *graphics = GetGraphicsContext();
        return graphics && graphics->LoadMaterialRecipeFile(path);
    }

    bool WorkObject::LoadRecipeDirectory(const OSString &path)
    {
        auto *graphics = GetGraphicsContext();
        return graphics && graphics->LoadMaterialRecipeDirectory(path);
    }

    void WorkObject::SetClearColor(const Color4f &color)
    {
        // 清屏色唯一权威在渲染目标上（RenderTargetDesc::clear_color）。
        // 写主世界绑定的 RT；world 未就绪时丢弃（Init 前不应设置）。
        if (world)
            if (auto *rt = world->GetRenderTarget())
                rt->SetClearColor(color);
    }

    const VkExtent2D *WorkObject::GetExtent()
    {
        if (world)
        {
            auto target = world->GetRenderTarget();
            return target ? &target->GetExtent() : nullptr;
        }

        return nullptr;
    }

    const graph::ViewportInfo *WorkObject::GetViewportInfo() const
    {
        if (world)
        {
            auto camera_system = world->GetSystem<ecs::CameraSystem>();
            if (camera_system)
                return camera_system->GetViewportInfo();

            auto target = world->GetRenderTarget();
            return target ? target->GetViewportInfo() : nullptr;
        }

        return nullptr;
    }

    const math::Vector2i *WorkObject::GetMouseCoord() const
    {
        if (!world)
            return nullptr;

        auto input_system = world->GetSystem<ecs::InputSystem>();
        return input_system ? &input_system->GetMouseCoord() : nullptr;
    }

    void WorkObject::Tick(double delta)
    {
        if (world)
            world->Tick(static_cast<float>(delta));
    }

    // WorkObject::OnRenderPass：渲染帧内录制钩子（BeginRenderPass 之后、
    // 系统绘制之前），基类无操作。

    // Resource helpers removed. Use RenderContext/GraphicsContext directly.
}//namespace hgl
