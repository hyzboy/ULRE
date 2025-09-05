#pragma once

#include<hgl/platform/Window.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/module/SwapchainModule.h>
#include<hgl/graph/module/GraphModuleManager.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/graph/RenderList.h>
#include<hgl/graph/CameraControl.h>
#include<hgl/graph/Renderer.h>
#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/mtl/MaterialLibrary.h>
#include<hgl/io/event/MouseEvent.h>
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

class Scene;
class Renderer;

class CameraComponentManager{/*现阶段测试使用*/};
class LightComponentManager{/*现阶段测试使用*/};

struct RenderWorkspace
{
    Scene *         scene       =nullptr;
    Camera *        camera      =nullptr;
    Renderer *      renderer    =nullptr;
};

class RenderFramework:public io::WindowEvent
{
    OSString                app_name;

    Window *                win                 =nullptr;
    VulkanInstance *        inst                =nullptr;

    VulkanDevice *          device              =nullptr;

    RenderResource *        render_resource     =nullptr;

protected:

    GraphModuleManager *    module_manager      =nullptr;

    RenderPassManager *     rp_manager          =nullptr;

    TextureManager *        tex_manager         =nullptr;
    RenderTargetManager *   rt_manager          =nullptr;
    MaterialManager *       material_manager    =nullptr;
    BufferManager *         buffer_manager      =nullptr;
    SamplerManager *        sampler_manager     =nullptr;

    SwapchainModule *       sc_module           =nullptr;

protected:

    CameraComponentManager *camera_component_manager=nullptr;
    LightComponentManager  *light_component_manager =nullptr;

protected:  //RenderContext,未来合并成一个RenderContext结构

    Scene *                 default_scene           =nullptr;
    Camera *                default_camera          =nullptr;
    CameraControl *         default_camera_control  =nullptr;
    Renderer *              default_renderer        =nullptr;

    void OnChangeDefaultScene(Scene *);

    void CreateDefaultRenderer();

protected:  //EventDispatcher

    io::MouseEvent *mouse_event=nullptr;

public:

            Window *            GetWindow           ()const{return win;}
            VulkanDevice *      GetDevice           ()const{return device;}
            VkDevice            GetVkDevice         ()const{return device->GetDevice();}
    const   VulkanPhyDevice *   GetPhyDevice        ()const{return device->GetPhyDevice();}
            VulkanDevAttr *     GetDevAttr          ()const{return device->GetDevAttr();}
            VulkanSurface *     GetSurface          ()const{return device->GetDevAttr()->surface;}
            RenderResource *    GetRenderResource   ()const{return render_resource;}

public:

    GraphModuleManager *    GetModuleManager        (){return module_manager;}

    RenderPassManager *     GetRenderPassManager    (){return rp_manager;}
    TextureManager *        GetTextureManager       (){return tex_manager;}
    RenderTargetManager *   GetRenderTargetManager  (){return rt_manager;}
    MaterialManager *       GetMaterialManager      (){return material_manager;}
    BufferManager *         GetBufferManager        (){return buffer_manager;}
    SamplerManager *        GetSamplerManager       (){return sampler_manager;}

    SwapchainModule *       GetSwapchainModule      (){return sc_module;}
    SwapchainRenderTarget * GetSwapchainRenderTarget(){return sc_module?sc_module->GetRenderTarget():nullptr;}

public:

    Scene *                 GetDefaultScene         (){return default_scene;}
    Camera *                GetDefaultCamera        (){return default_camera;}
    CameraControl *         GetDefaultCameraControl (){return default_camera_control;}
    Renderer *              GetDefaultRenderer      (){return default_renderer;}

    RenderPass *            GetDefaultRenderPass    (){return default_renderer->GetRenderPass();}

public:

    bool GetMouseCoord(Vector2i *mc)const
    {
        if(!mouse_event||!mc)
            return(false);

        *mc=mouse_event->GetMouseCoord();
        return(true);
    }

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

    RenderList *CreateRenderList()
    {
        return(new RenderList(device));
    }

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

    SharedPtr<graph::PrimitiveCreater> GetPrimitiveCreater(graph::Material *mtl)
    {
        if(!mtl)
            return(nullptr);

        return(new graph::PrimitiveCreater(GetDevice(),mtl->GetDefaultVIL()));
    }

    SharedPtr<graph::PrimitiveCreater> GetPrimitiveCreater(graph::MaterialInstance *mi)
    {
        if(!mi)
            return(nullptr);

        return(new graph::PrimitiveCreater(GetDevice(),mi->GetVIL()));
    }

    graph::VertexDataManager *CreateVDM(const graph::VIL *vil,const VkDeviceSize vertices_number,VkDeviceSize indices_number,const IndexType type=IndexType::U16);
    graph::VertexDataManager *CreateVDM(const graph::VIL *vil,const VkDeviceSize number,const IndexType type=IndexType::U16){return CreateVDM(vil,number,number,type);}

public: // Primitive, Mesh

    graph::Primitive *CreatePrimitive(const AnsiString &name,
                                      const uint32_t vertices_count,
                                      const graph::VIL *vil,
                                      const std::initializer_list<graph::VertexAttribDataPtr> &vad_list);

    graph::Mesh *CreateMesh(const AnsiString &name,
                            const uint32_t vertices_count,
                            graph::MaterialInstance *mi,
                            graph::Pipeline *pipeline,
                            const std::initializer_list<graph::VertexAttribDataPtr> &vad_list);

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
            //        LOG_ERROR(OS_TEXT("CreateComponent failed, no default manager!"));
            return(nullptr);
        }

        C *c=(C *)(manager->CreateComponent(args...)); //创建组件

        if(!c)
        {
            //        LOG_ERROR(OS_TEXT("CreateComponent failed, create component failed!"));
            return(nullptr);
        }

        /**
        * 如果此处出现转换错误，请检查是否包含了对应的Component头文件。
        */
        if(cci)
        {
            if(cci->owner_node)
                cci->owner_node->AttachComponent(c); //将组件附加到所属节点

            c->graph::SceneOrient::SetLocalMatrix(cci->mat);
        }

        return c;
    }
};//class RenderFramework

VK_NAMESPACE_END
