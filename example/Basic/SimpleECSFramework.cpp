// 简化的ECS框架实现
// Simplified ECS Framework Implementation

#include"SimpleECSFramework.h"
#include<hgl/graph/VKRenderTargetSwapchain.h>
#include<hgl/graph/VKRenderPass.h>
#include<hgl/graph/VKFramebuffer.h>
#include<hgl/graph/module/RenderPassManager.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/RenderTargetManager.h>
#include<hgl/graph/mtl/MaterialCreateInfo.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>

namespace hgl
{
    // ========== SimpleECSFramework Implementation ==========
    
    SimpleECSFramework::SimpleECSFramework(const OSString& title)
        : app_name(title)
    {
    }
    
    SimpleECSFramework::~SimpleECSFramework()
    {
        SAFE_CLEAR(sc_module);
        SAFE_CLEAR(primitive_manager);
        SAFE_CLEAR(geometry_manager);
        SAFE_CLEAR(sampler_manager);
        SAFE_CLEAR(buffer_manager);
        SAFE_CLEAR(material_manager);
        SAFE_CLEAR(rt_manager);
        SAFE_CLEAR(tex_manager);
        SAFE_CLEAR(rp_manager);
        SAFE_CLEAR(module_manager);
        SAFE_CLEAR(device);
        SAFE_CLEAR(inst);
        SAFE_CLEAR(win);
    }
    
    bool SimpleECSFramework::Init(uint w, uint h)
    {
        // 创建窗口
        win = hglCreateWindow(app_name, 0, 0, w, h, io::WindowSetup());
        if (!win)
            return false;
        
        win->AddChildDispatcher(this);
        
        // 创建Vulkan实例
        inst = CreateVulkanInstance(app_name);
        if (!inst)
            return false;
        
        // 创建设备
        VkSurfaceKHR surface;
        if (!win->CreateSurface(inst->GetInstance(), &surface))
            return false;
        
        device = CreateVulkanDevice(inst, surface);
        if (!device)
            return false;
        
        // 创建模块管理器
        module_manager = new graph::GraphModuleManager(device);
        
        // 创建各种管理器
        rp_manager = new graph::RenderPassManager(device);
        tex_manager = new graph::TextureManager(device);
        rt_manager = new graph::RenderTargetManager(device, tex_manager);
        material_manager = new graph::MaterialManager(device);
        buffer_manager = new graph::BufferManager(device);
        sampler_manager = new graph::SamplerManager(device);
        geometry_manager = new graph::GeometryManager(device);
        primitive_manager = new graph::PrimitiveManager(device, geometry_manager);
        
        // 创建SwapchainModule
        sc_module = new graph::SwapchainModule(device);
        if (!sc_module->Init(w, h))
            return false;
        
        return true;
    }
    
    io::EventProcResult SimpleECSFramework::OnEvent(const io::EventHeader& header, const uint64 data)
    {
        if (header.type == io::EventType::MouseMove)
        {
            const io::EventMouseMove* emm = (const io::EventMouseMove*)data;
            mouse_coord.x = emm->x;
            mouse_coord.y = emm->y;
        }
        
        return io::EventProcResult::Normal;
    }
    
    // ========== SimpleECSWorkObject Implementation ==========
    
    graph::Material* SimpleECSWorkObject::CreateMaterial(const OSString& name, graph::mtl::MaterialCreateInfo* mci)
    {
        if (!framework || !mci)
            return nullptr;
        
        auto mat_mgr = framework->GetMaterialManager();
        if (!mat_mgr)
            return nullptr;
        
        return mat_mgr->Create(name, mci);
    }
    
    graph::MaterialInstance* SimpleECSWorkObject::CreateMaterialInstance(graph::Material* mtl)
    {
        if (!framework || !mtl)
            return nullptr;
        
        auto mat_mgr = framework->GetMaterialManager();
        if (!mat_mgr)
            return nullptr;
        
        return mat_mgr->CreateInstance(mtl);
    }
    
    graph::MaterialInstance* SimpleECSWorkObject::CreateMaterialInstance(
        const OSString& mtl_name,
        graph::mtl::Material2DCreateConfig* cfg,
        graph::VILConfig* vil_config)
    {
        if (!framework || !cfg)
            return nullptr;
        
        auto mat_mgr = framework->GetMaterialManager();
        if (!mat_mgr)
            return nullptr;
        
        // 这里需要根据实际的材质创建流程来实现
        // 简化版本，直接返回nullptr，具体实现需要参考原有代码
        return nullptr;
    }
    
    graph::Pipeline* SimpleECSWorkObject::CreatePipeline(graph::MaterialInstance* mi, const OSString& pipeline_name)
    {
        if (!framework || !mi)
            return nullptr;
        
        auto sc_module = framework->GetSwapchainModule();
        if (!sc_module)
            return nullptr;
        
        // 使用swapchain的render target创建pipeline
        return mi->CreatePipeline(sc_module->GetRenderTarget(), pipeline_name);
    }
    
    graph::Geometry* SimpleECSWorkObject::CreateGeometry(
        const OSString& name,
        uint vertex_count,
        graph::VertexInputLayout* vil,
        const graph::VertexBufferDataList& vb_list)
    {
        if (!framework)
            return nullptr;
        
        auto geom_mgr = framework->GetGeometryManager();
        if (!geom_mgr)
            return nullptr;
        
        return geom_mgr->Create(name, vertex_count, vil, vb_list);
    }
    
    graph::Primitive* SimpleECSWorkObject::CreatePrimitive(
        const OSString& name,
        uint vertex_count,
        graph::MaterialInstance* mi,
        graph::Pipeline* pl,
        const graph::VertexBufferDataList& vb_list)
    {
        if (!framework || !mi || !pl)
            return nullptr;
        
        auto prim_mgr = framework->GetPrimitiveManager();
        if (!prim_mgr)
            return nullptr;
        
        return prim_mgr->Create(name, vertex_count, mi, pl, vb_list);
    }
    
    graph::Primitive* SimpleECSWorkObject::CreatePrimitive(
        graph::Geometry* geom,
        graph::MaterialInstance* mi,
        graph::Pipeline* pl)
    {
        if (!framework || !geom || !mi || !pl)
            return nullptr;
        
        auto prim_mgr = framework->GetPrimitiveManager();
        if (!prim_mgr)
            return nullptr;
        
        return prim_mgr->Create(geom, mi, pl);
    }
    
    // ========== SimpleECSWorkManager Implementation ==========
    
    SimpleECSWorkManager::SimpleECSWorkManager(SimpleECSFramework* rf)
        : framework(rf)
        , swapchain_module(rf->GetSwapchainModule())
    {
        rf->AddChildDispatcher(this);
    }
    
    SimpleECSWorkManager::~SimpleECSWorkManager()
    {
        framework->RemoveChildDispatcher(this);
        if (cur_work_object)
            OnChangeWorkObject(cur_work_object, nullptr);
        SAFE_CLEAR(cur_work_object);
    }
    
    void SimpleECSWorkManager::Tick(SimpleECSWorkObject* wo)
    {
        if (!wo)
            return;
        
        cur_time = GetDoubleTime();
        
        if (cur_time - last_update_time >= frame_time)
        {
            wo->Tick(cur_time - last_update_time);
            last_update_time = cur_time;
        }
    }
    
    void SimpleECSWorkManager::Render(SimpleECSWorkObject* wo)
    {
        if (!wo || !swapchain_module)
            return;
        
        // 获取swapchain图像
        uint32_t image_index;
        if (!swapchain_module->AcquireNextImage(&image_index))
            return;
        
        // 获取命令缓冲
        auto cmd_buf = swapchain_module->GetCmdBuffer(image_index);
        if (!cmd_buf)
            return;
        
        // 开始渲染
        cmd_buf->Begin();
        
        // 开始render pass
        auto render_target = swapchain_module->GetRenderTarget();
        auto framebuffer = swapchain_module->GetFramebuffer(image_index);
        
        cmd_buf->BeginRenderPass(render_target->GetRenderPass(), framebuffer);
        
        // 调用工作对象的绘制函数
        wo->Draw(cmd_buf);
        
        // 结束render pass
        cmd_buf->EndRenderPass();
        cmd_buf->End();
        
        // 提交并呈现
        swapchain_module->Submit(cmd_buf);
        swapchain_module->Present(image_index);
    }
    
    void SimpleECSWorkManager::Run(SimpleECSWorkObject* wo)
    {
        if (!wo)
            return;
        
        cur_work_object = wo;
        OnChangeWorkObject(nullptr, wo);
        
        last_update_time = GetDoubleTime();
        last_render_time = last_update_time;
        
        auto win = framework->GetWindow();
        
        while (win && win->IsOpen() && !wo->IsDestroy())
        {
            win->ProcEvent();
            
            Tick(wo);
            Render(wo);
        }
    }
    
    void SimpleECSWorkManager::OnResize(uint w, uint h)
    {
        if (swapchain_module)
        {
            swapchain_module->Recreate(w, h);
        }
        
        if (cur_work_object)
        {
            VkExtent2D extent = {w, h};
            cur_work_object->OnResize(extent);
        }
    }
    
    void SimpleECSWorkManager::OnChangeWorkObject(SimpleECSWorkObject* old_work, SimpleECSWorkObject* new_work)
    {
        // 子类可以重写此函数来处理工作对象切换
        // Subclasses can override this function to handle work object switching
    }
    
} // namespace hgl
