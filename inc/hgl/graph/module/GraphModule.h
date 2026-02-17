#pragma once

#include<hgl/vk/VK.h>
#include<hgl/graph/GraphTypes.h>
#include<hgl/type/TypeInfo.h>

VK_NAMESPACE_BEGIN

class TextureManager;
class RenderTargetManager;
class RenderPassManager;
class GraphicsContext;

class GraphModule
{
    GraphicsContext *graphics_context=nullptr;

public:

                GraphicsContext *   GetGraphicsContext  ()const{return graphics_context;}              ///<取得GraphicsContext
                void                SetGraphicsContext  (GraphicsContext *gc)
                {
                    graphics_context=gc;
                    OnGraphicsContextChanged(gc);
                }   ///<设置GraphicsContext

                VulkanDevice *      GetDevice           ();                                             ///<取得GPU设备
                VkDevice            GetVkDevice         ()const;                                        ///<取得VkDevice
        const   VulkanPhyDevice *   GetPhyDevice        ()const;                                        ///<取得物理设备
                VulkanDevAttr *     GetDevAttr          ()const;                                        ///<取得设备属性
                VulkanSurface *     GetSurface          ()const;                                        ///<取得表面
                VkPipelineCache     GetPipelineCache    ()const;                                        ///<取得PipelineCache

public:

    virtual void OnResize(const VkExtent2D &){};                                                    ///<窗口大小改变

    /**
     * @brief 清理模块资源 - 在销毁前调用，用于释放模块持有的GPU资源
     * @warning GraphModuleManager会在销毁时自动调用此方法
     * 各个子模块应在此方法中清理残留的GPU资源（缓冲区、纹理等）
     */
    virtual void Release(){}

protected:

    virtual void OnGraphicsContextChanged(GraphicsContext *)    {}

public:

    GraphModule(GraphicsContext *gc):graphics_context(gc){}
    virtual ~GraphModule()=default;

    virtual const size_t GetTypeHash()const noexcept=0;
    virtual const AnsiString &GetName()const=0;
};//class GraphModule

template<typename T,typename BASE> class GraphModuleInherit:public BASE
{
    AnsiString manager_name;

public:

    const size_t GetTypeHash()const noexcept override
    {
        return typeid(T).hash_code();
    }

    const AnsiString &GetName()const override
    {
        return manager_name;
    }

public:

    GraphModuleInherit(GraphicsContext *gc,const AnsiString &name):BASE(gc)
    {
        manager_name=name;
    }

    virtual ~GraphModuleInherit()=default;
};//class GraphModuleInherit

#define GRAPH_MODULE_CLASS(class_name) class class_name:public GraphModuleInherit<class_name,GraphModule>

#define GRAPH_MODULE_CONSTRUCT(class_name) class_name::class_name(GraphicsContext *gc):GraphModuleInherit<class_name,GraphModule>(gc,#class_name)

VK_NAMESPACE_END
