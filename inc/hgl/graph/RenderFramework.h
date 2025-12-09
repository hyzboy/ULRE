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
#include<hgl/graph/World.h>
#include<hgl/graph/SceneRenderer.h>
#include<hgl/graph/GeometryCreater.h>
#include<hgl/component/CreateComponentInfo.h>

VK_NAMESPACE_BEGIN

namespace mtl
{
    struct Material2DCreateConfig;
    struct Material3DCreateConfig;
    class MaterialCreateInfo;
}//namespace mtl

class FontDataSource;
class TileFont;
class FontSource;
class RenderPassManager;
class TextureManager;
class RenderTargetManager;
class MaterialManager;
class BufferManager;

class RenderModule;

class World;
class SceneRenderer;
class LineRenderManager; // forward

class CameraComponentManager{/*现阶段测试使用*/};
class LightComponentManager{/*现阶段测试使用*/};

class RenderFramework:public io::WindowEvent
{
    OSString                app_name;

    Window *                win                 =nullptr;
    VulkanInstance *        inst                =nullptr;

    VulkanDevice *          device              =nullptr;

protected:

    GraphModuleManager *    module_manager      =nullptr;

    RenderPassManager *     rp_manager          =nullptr;

    TextureManager *        tex_manager         =nullptr;
    RenderTargetManager *   rt_manager          =nullptr;
    MaterialManager *       material_manager    =nullptr;
    BufferManager *         buffer_manager      =nullptr;
    SamplerManager *        sampler_manager     =nullptr;
    GeometryManager *       geometry_manager    =nullptr;
    PrimitiveManager *      primitive_manager   =nullptr;

    SwapchainModule *       sc_module           =nullptr;

protected:

    CameraComponentManager *camera_component_manager=nullptr;
    LightComponentManager  *light_component_manager =nullptr;

protected:  //RenderContext,未来合并成一个RenderContext结构

    World *                 default_world           =nullptr;
    SceneRenderer *         default_scene_renderer  =nullptr;

    void OnChangeDefaultWorld(World *);

    void CreateDefaultSceneRenderer();

protected:  //EventDispatcher

    Vector2i mouse_coord;

    virtual io::EventProcResult OnEvent(const io::EventHeader &header,const uint64 data) override;

public:

            Window *            GetWindow           ()const{return win;}
            VulkanDevice *      GetDevice           ()const{return device;}
            VkDevice            GetVkDevice         ()const{return device->GetDevice();}
    const   VulkanPhyDevice *   GetPhyDevice        ()const{return device->GetPhyDevice();}
            VulkanDevAttr *     GetDevAttr          ()const{return device->GetDevAttr();}
            VulkanSurface *     GetSurface          ()const{return device->GetDevAttr()->surface;}

public:

    GraphModuleManager *    GetModuleManager        (){return module_manager;}

    RenderPassManager *     GetRenderPassManager    (){return rp_manager;}
    TextureManager *        GetTextureManager       (){return tex_manager;}
    RenderTargetManager *   GetRenderTargetManager  (){return rt_manager;}
    MaterialManager *       GetMaterialManager      (){return material_manager;}
    BufferManager *         GetBufferManager        (){return buffer_manager;}
    SamplerManager *        GetSamplerManager       (){return sampler_manager;}
    GeometryManager *       GetGeometryManager      (){return geometry_manager;}
    PrimitiveManager *      GetPrimitiveManager     (){return primitive_manager;}

    SwapchainModule *       GetSwapchainModule      (){return sc_module;}
    SwapchainRenderTarget * GetSwapchainRenderTarget(){return sc_module?sc_module->GetRenderTarget():nullptr;}

public:

    World *                 GetDefaultWorld         (){return default_world;}
    SceneRenderer *         GetDefaultSceneRenderer (){return default_scene_renderer;}

    RenderPass *            GetDefaultRenderPass    (){return default_scene_renderer->GetRenderPass();}

    LineRenderManager *     GetLineRenderManager    (){return default_scene_renderer?default_scene_renderer->GetLineRenderManager():nullptr;}

public:

    const Vector2i &GetMouseCoord()const{ return mouse_coord; }

public:

    RenderFramework(const OSString &);
    virtual ~RenderFramework();

    virtual bool Init(uint w,uint h);

public: // event

    virtual void OnResize(uint w,uint h);
    virtual void OnActive(bool);
    virtual void OnClose();

public:

    void Tick();

public: // other

    //RenderCollector *CreateRenderCollector()
    //{
    //    return(new RenderCollector(device));
    //}

    TileFont *CreateTileFont(FontSource *fs,int limit_count=-1);                                                     ///<创建只使用一种字符的Tile字符管理对象
    TextRender *CreateTextRender(graph::FontSource *fs,const int limit_count=1024);

    TextRender *CreateTextRender(const OSString &latin_font,const OSString &cjk_font,const int size,const int limit_count=1024);

public:

    template<typename ...ARGS>
    graph::Pipeline *CreatePipeline(ARGS...args)
    {
        return GetDefaultRenderPass()->CreatePipeline(args...);
    }

public:

    template<typename ...ARGS>
    graph::Material *CreateMaterial(ARGS...args)
    {
        return material_manager->CreateMaterial(args...);
    }

    template<typename ...ARGS>
    graph::Material *LoadMaterial(ARGS...args)
    {
        return material_manager->LoadMaterial(args...);
    }

    template<typename ...ARGS>
    graph::MaterialInstance *CreateMaterialInstance(ARGS...args)
    {
        return material_manager->CreateMaterialInstance(args...);
    }

public:

    template<typename ...ARGS>  graph::VAB *CreateVAB(ARGS...args){return buffer_manager->CreateVAB(args...);}

    template<typename ...ARGS>  graph::DeviceBuffer *CreateUBO(ARGS...args) { return buffer_manager->CreateUBO(args...); }
    template<typename ...ARGS>  graph::DeviceBuffer *CreateSSBO(ARGS...args) { return buffer_manager->CreateSSBO(args...); }
    template<typename ...ARGS>  graph::DeviceBuffer *CreateINBO(ARGS...args) { return buffer_manager->CreateINBO(args...); }

    template<typename ...ARGS>  graph::IndexBuffer *CreateIBO(ARGS...args) { return buffer_manager->CreateIBO(args...); }
    template<typename ...ARGS>  graph::IndexBuffer *CreateIBO8(ARGS...args) { return buffer_manager->CreateIBO8(args...); }
    template<typename ...ARGS>  graph::IndexBuffer *CreateIBO16(ARGS...args) { return buffer_manager->CreateIBO16(args...); }
    template<typename ...ARGS>  graph::IndexBuffer *CreateIBO32(ARGS...args) { return buffer_manager->CreateIBO32(args...); }

    template<typename T,typename ...ARGS> T *CreateUBO(ARGS...args) { return device->CreateUBO<T>(args...); }
    template<typename T,typename ...ARGS> T *CreateSSBO(ARGS...args) { return device->CreateSSBO<T>(args...); }
    template<typename T,typename ...ARGS> T *CreateINBO(ARGS...args) { return device->CreateINBO<T>(args...); }

public:

    SharedPtr<graph::GeometryCreater> GetGeometryCreater(graph::Material *mtl)
    {
        if(!mtl)
            return(nullptr);

        return(new graph::GeometryCreater(GetDevice(),mtl->GetDefaultVIL()));
    }

    SharedPtr<graph::GeometryCreater> GetGeometryCreater(graph::MaterialInstance *mi)
    {
        if(!mi)
            return(nullptr);

        return(new graph::GeometryCreater(GetDevice(),mi->GetVIL()));
    }

    graph::VertexDataManager *CreateVDM(const graph::VIL *vil,const VkDeviceSize vertices_number,VkDeviceSize indices_number,const IndexType type=IndexType::U16);
    graph::VertexDataManager *CreateVDM(const graph::VIL *vil,const VkDeviceSize number,const IndexType type=IndexType::U16){return CreateVDM(vil,number,number,type);}

public: // Geometry, Primitive

    graph::Geometry *CreateGeometry(const AnsiString &name,
                                      const uint32_t vertices_count,
                                      const graph::VIL *vil,
                                      const std::initializer_list<graph::VertexAttribDataPtr> &vad_list);

    graph::Primitive *CreatePrimitive(const AnsiString &name,
                            const uint32_t vertices_count,
                            graph::MaterialInstance *mi,
                            graph::Pipeline *pipeline,
                            const std::initializer_list<graph::VertexAttribDataPtr> &vad_list);


    Primitive *CreatePrimitive(Geometry *r, MaterialInstance *mi, Pipeline *p){return primitive_manager->CreatePrimitive(r,mi,p);}    
    Primitive *CreatePrimitive(GeometryCreater *pc, MaterialInstance *mi, Pipeline *p){return primitive_manager->CreatePrimitive(pc,mi,p);}    

public: // ComponentManager

    template<typename T> T *GetComponentManager()
    {
        return COMPONENT_NAMESPACE::GetComponentManager<T>(true);
    }

    template<> CameraComponentManager *GetComponentManager<CameraComponentManager>()
    {
        return camera_component_manager;
    }

    template<> LightComponentManager *GetComponentManager<LightComponentManager>()
    {
        return light_component_manager;
    }

public: //Component 相关

    template<typename C,typename ...ARGS>
    inline C *CreateComponent(const CreateComponentInfo *cci,ARGS...args)
    {
        auto manager=C::GetDefaultManager();  //取得默认管理器

        if(!manager)
        {
            //        LogError(OS_TEXT("CreateComponent failed, no default manager!"));
            return(nullptr);
        }

        C *c=(C *)(manager->CreateComponent(args...)); //创建组件

        if(!c)
        {
            //        LogError(OS_TEXT("CreateComponent failed, create component failed!"));
            return(nullptr);
        }

        /**
        * 如果此处出现转换错误，请检查是否包含了对应的Component头文件。
        */
        if(cci)
        {
            if(cci->owner_node)
                cci->owner_node->AttachComponent(c); //将组件附加到所属节点

            c->graph::NodeTransform::SetLocalMatrix(cci->mat);
        }

        return c;
    }
};//class RenderFramework

VK_NAMESPACE_END
