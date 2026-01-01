// 简化的ECS框架 - 移除了World和SceneRenderer依赖
// Simplified ECS Framework - Removed World and SceneRenderer dependencies
//
// 这是RenderFramework/WorkObject/WorkManager的简化版本
// 专门为ECS渲染设计，不包含传统的SceneGraph组件
//
// This is a simplified version of RenderFramework/WorkObject/WorkManager
// Designed specifically for ECS rendering, without traditional SceneGraph components

#pragma once

#include<hgl/platform/Window.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/module/SwapchainModule.h>
#include<hgl/graph/module/GraphModuleManager.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/GeometryCreater.h>
#include<hgl/time/Time.h>
#include<hgl/type/object/TickObject.h>

namespace hgl
{
    namespace graph
    {
        class Texture2D;
        class Texture2DArray;
        class TextureCube;
        class Geometry;
        class Sampler;
        class Texture;
        class RenderPassManager;
        class TextureManager;
        class RenderTargetManager;
        
        namespace mtl
        {
            struct Material2DCreateConfig;
            struct Material3DCreateConfig;
            class MaterialCreateInfo;
        }
    }
    
    /**
     * 简化的ECS渲染框架
     * Simplified ECS Render Framework
     * 
     * 移除了World和SceneRenderer，只保留基础的Vulkan和资源管理
     * Removed World and SceneRenderer, keeping only basic Vulkan and resource management
     */
    class SimpleECSFramework : public io::WindowEvent
    {
        OSString app_name;
        
        Window* win = nullptr;
        VulkanInstance* inst = nullptr;
        VulkanDevice* device = nullptr;
        
    protected:
        graph::GraphModuleManager* module_manager = nullptr;
        graph::RenderPassManager* rp_manager = nullptr;
        graph::TextureManager* tex_manager = nullptr;
        graph::RenderTargetManager* rt_manager = nullptr;
        graph::MaterialManager* material_manager = nullptr;
        graph::BufferManager* buffer_manager = nullptr;
        graph::SamplerManager* sampler_manager = nullptr;
        graph::GeometryManager* geometry_manager = nullptr;
        graph::PrimitiveManager* primitive_manager = nullptr;
        graph::SwapchainModule* sc_module = nullptr;
        
        Vector2i mouse_coord;
        
        virtual io::EventProcResult OnEvent(const io::EventHeader& header, const uint64 data) override;
        
    public:
        SimpleECSFramework(const OSString& title);
        virtual ~SimpleECSFramework();
        
        bool Init(uint w, uint h);
        
    public: // 访问器 Accessors
        Window* GetWindow() const { return win; }
        VulkanDevice* GetDevice() const { return device; }
        VkDevice GetVkDevice() const { return device->GetDevice(); }
        const VulkanPhyDevice* GetPhyDevice() const { return device->GetPhyDevice(); }
        VulkanDevAttr* GetDevAttr() const { return device->GetDevAttr(); }
        VulkanSurface* GetSurface() const { return device->GetDevAttr()->surface; }
        
        graph::TextureManager* GetTextureManager() const { return tex_manager; }
        graph::BufferManager* GetBufferManager() const { return buffer_manager; }
        graph::SamplerManager* GetSamplerManager() const { return sampler_manager; }
        graph::GeometryManager* GetGeometryManager() const { return geometry_manager; }
        graph::PrimitiveManager* GetPrimitiveManager() const { return primitive_manager; }
        graph::MaterialManager* GetMaterialManager() const { return material_manager; }
        graph::SwapchainModule* GetSwapchainModule() const { return sc_module; }
        
        const Vector2i& GetMouseCoord() const { return mouse_coord; }
        const VkExtent2D* GetExtent() const { return sc_module ? &sc_module->GetExtent() : nullptr; }
    };
    
    /**
     * 简化的ECS工作对象
     * Simplified ECS Work Object
     * 
     * 不包含World和SceneRenderer的引用
     * Does not include references to World and SceneRenderer
     */
    class SimpleECSWorkObject : public TickObject
    {
    protected:
        SimpleECSFramework* framework = nullptr;
        bool destroy_flag = false;
        bool render_dirty = true;
        
    public:
        SimpleECSWorkObject(SimpleECSFramework* rf) : framework(rf) {}
        virtual ~SimpleECSWorkObject() = default;
        
        // 访问器 Accessors
        SimpleECSFramework* GetFramework() { return framework; }
        graph::VulkanDevice* GetDevice() { return framework ? framework->GetDevice() : nullptr; }
        graph::VulkanDevAttr* GetDevAttr() { return framework ? framework->GetDevAttr() : nullptr; }
        graph::TextureManager* GetTextureManager() { return framework ? framework->GetTextureManager() : nullptr; }
        graph::BufferManager* GetBufferManager() { return framework ? framework->GetBufferManager() : nullptr; }
        const VkExtent2D* GetExtent() { return framework ? framework->GetExtent() : nullptr; }
        const Vector2i* GetMouseCoord() const { return framework ? &framework->GetMouseCoord() : nullptr; }
        
        // 生命周期 Lifecycle
        bool IsDestroy() const { return destroy_flag; }
        void MarkDestroy() { destroy_flag = true; }
        bool IsRenderDirty() const { return render_dirty; }
        void MarkRenderDirty() { render_dirty = true; }
        
        // 虚函数 Virtual functions
        virtual bool Init() = 0;
        virtual void OnResize(const VkExtent2D&) {}
        virtual void Tick(double delta_time) override {}
        virtual void Draw(graph::RenderCmdBuffer* cmd_buf) {}
        
        // 资源创建辅助函数 Resource creation helpers
        graph::Material* CreateMaterial(const OSString& name, graph::mtl::MaterialCreateInfo* mci);
        graph::MaterialInstance* CreateMaterialInstance(graph::Material* mtl);
        graph::MaterialInstance* CreateMaterialInstance(const OSString& mtl_name, graph::mtl::Material2DCreateConfig* cfg, graph::VILConfig* vil_config = nullptr);
        graph::Pipeline* CreatePipeline(graph::MaterialInstance* mi, const OSString& pipeline_name);
        graph::Geometry* CreateGeometry(const OSString& name, uint vertex_count, graph::VertexInputLayout* vil, const graph::VertexBufferDataList& vb_list);
        graph::Primitive* CreatePrimitive(const OSString& name, uint vertex_count, graph::MaterialInstance* mi, graph::Pipeline* pl, const graph::VertexBufferDataList& vb_list);
        graph::Primitive* CreatePrimitive(graph::Geometry* geom, graph::MaterialInstance* mi, graph::Pipeline* pl);
    };
    
    /**
     * 简化的ECS工作管理器
     * Simplified ECS Work Manager
     */
    class SimpleECSWorkManager : public io::WindowEvent
    {
    protected:
        SimpleECSFramework* framework;
        graph::SwapchainModule* swapchain_module;
        
        uint fps = 60;
        double frame_time = 1.0 / 60.0;
        double last_update_time = 0;
        double last_render_time = 0;
        double cur_time = 0;
        
        SimpleECSWorkObject* cur_work_object = nullptr;
        
    public:
        SimpleECSWorkManager(SimpleECSFramework* rf);
        virtual ~SimpleECSWorkManager();
        
        void SetFPS(uint f)
        {
            fps = f;
            frame_time = 1.0 / double(fps);
        }
        
        void Tick(SimpleECSWorkObject* wo);
        void Render(SimpleECSWorkObject* wo);
        void Run(SimpleECSWorkObject* wo);
        
        void OnResize(uint w, uint h) override;
        
        virtual void OnChangeWorkObject(SimpleECSWorkObject* old_work, SimpleECSWorkObject* new_work);
    };
    
    /**
     * 简化的ECS运行框架模板函数
     * Simplified ECS Run Framework Template Function
     */
    template<typename WO>
    int RunSimpleECSFramework(const OSString& title, uint width = 1280, uint height = 720)
    {
        SimpleECSFramework framework(title);
        
        if (!framework.Init(width, height))
            return -1;
        
        SimpleECSWorkManager manager(&framework);
        
        WO* wo = new WO(&framework);
        
        if (!wo->Init())
        {
            delete wo;
            return -2;
        }
        
        manager.Run(wo);
        
        delete wo;
        return 0;
    }
    
} // namespace hgl
