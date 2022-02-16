#include<hgl/type/StringList.h>
#include<hgl/graph/TextureLoader.h>
#include<hgl/graph/TileData.h>
#include<hgl/graph/font/TileFont.h>
#include<hgl/graph/font/TextLayout.h>
#include<hgl/graph/font/TextRenderable.h>

#include"VulkanAppFramework.h"
#include<hgl/graph/VKTexture.h>
#include<hgl/graph/VKSampler.h>
#include<hgl/math/Math.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH =1280;
constexpr uint32_t SCREEN_HEIGHT=SCREEN_WIDTH/16*9;

constexpr uint CHAR_BITMAP_SIZE=16;         //字符尺寸

class TextRender
{
    GPUDevice *         device              =nullptr;
    RenderResource *    db                  =nullptr;

    Material *          material            =nullptr;
    MaterialInstance *  material_instance   =nullptr;

    Sampler *           sampler             =nullptr;

    Pipeline *          pipeline            =nullptr;

    FontSource *        eng_fs              =nullptr;
    FontSource *        chs_fs              =nullptr;
    FontSourceMulti *   font_source         =nullptr;

    TileFont *          tile_font           =nullptr;
    TextLayout          tl_engine;                                      ///<文本排版引擎
    
    Color4f             color;
    GPUBuffer *         ubo_color           =nullptr;

public:

    TextRender(GPUDevice *dev)
    {
        device=dev;
        db=new RenderResource(device);
    }

    ~TextRender()
    {
        SAFE_CLEAR(tile_font);
        SAFE_CLEAR(db);
    }

private:    

    bool InitTileFont()
    {
        Font eng_fnt(OS_TEXT("Source Code Pro"),0,CHAR_BITMAP_SIZE);
        Font chs_fnt(OS_TEXT("微软雅黑"),0,CHAR_BITMAP_SIZE);

        eng_fs=AcquireFontSource(eng_fnt);
        chs_fs=AcquireFontSource(chs_fnt);

        font_source=new FontSourceMulti(eng_fs);
        font_source->AddCJK(chs_fs);

        tile_font=device->CreateTileFont(font_source);
        return(true);
    }

    bool InitTextLayoutEngine()
    {
        CharLayoutAttr cla;
        TextLayoutAttributes tla;

        cla.CharColor=Color4f(COLOR::White);
        cla.BackgroundColor=Color4f(COLOR::Black);

        tla.char_layout_attr=&cla;
        tla.line_gap=0.1f;

        tl_engine.SetFont(tile_font->GetFontSource());
        tl_engine.Set(&tla);
        tl_engine.SetTextDirection(0);
        tl_engine.SetAlign(TextAlign::Left);
        tl_engine.SetMaxWidth(0);
        tl_engine.SetMaxHeight(0);

        return tl_engine.Init();
    }

    bool InitUBO()
    {
        color.One();

        ubo_color=db->CreateUBO(sizeof(Color4f),&color);

        if(!ubo_color)
            return(false);

        return(true);
    }

    bool InitMaterial(RenderPass *rp,GPUBuffer *ubo_camera_info)
    {
        material=db->CreateMaterial(OS_TEXT("res/material/LumTextureRect2D"));

        //文本渲染Position坐标全部是使用整数，这里强制要求Position输入流使用RGBA16I格式
        {
            VABConfigInfo vab_config;
            VAConfig va_cfg;

            va_cfg.format=VF_V4I16;
            va_cfg.instance=false;

            vab_config.Add("Position",va_cfg);

            material_instance=db->CreateMaterialInstance(material,&vab_config);
            if(!material_instance)return(false);
        }

        pipeline=rp->CreatePipeline(material_instance,InlinePipeline::Solid2D,Prim::SolidRectangles);
        if(!pipeline)return(false);

        sampler=db->CreateSampler();
        
        {
            MaterialParameters *mp_global=material_instance->GetMP(DescriptorSetsType::Global);
        
            if(!mp_global)
                return(false);

            if(!mp_global->BindUBO("g_camera",ubo_camera_info))return(false);

            mp_global->Update();
        }

        {
            MaterialParameters *mp=material_instance->GetMP(DescriptorSetsType::Value);
        
            if(!mp)
                return(false);
            
            if(!mp->BindSampler("lum_texture",tile_font->GetTexture(),sampler))return(false);            
            if(!mp->BindUBO("color_material",ubo_color))return(false);

            mp->Update();
        }

        return(true);
    }

public:

    bool Init(RenderPass *rp,GPUBuffer *ubo_camera_info)
    {
        if(!InitTileFont())
            return(false);

        if(!InitTextLayoutEngine())
            return(false);

        if(!InitUBO())
            return(false);

        if(!InitMaterial(rp,ubo_camera_info))
            return(false);

        return(true);
    }

    TextRenderable *CreateRenderable()
    {
        return db->CreateTextRenderable(material);
    }

    RenderableInstance *CreateRenderableInstance(TextRenderable *text_render_obj,const UTF16String &str)
    {
        if(tl_engine.SimpleLayout(text_render_obj,tile_font,str)<=0)
            return(nullptr);

        return db->CreateRenderableInstance(text_render_obj,material_instance,pipeline);
    }
};//class TextRender

TextRender *CreateTextRender(GPUDevice *dev,RenderPass *rp,GPUBuffer *ubo_camera_info)
{
    if(!dev||!rp||!ubo_camera_info)
        return(nullptr);

    TextRender *text_render=new TextRender(dev);

    if(!text_render->Init(rp,ubo_camera_info))
    {
        delete text_render;
        return(nullptr);
    }

    return text_render;
}

class TestApp:public VulkanApplicationFramework
{
    Camera cam;

private:

    GPUBuffer *         ubo_camera_info     =nullptr;

private:

    TextRender *        text_render         =nullptr;

    TextRenderable *    text_render_obj     =nullptr;
    RenderableInstance *render_instance     =nullptr;

public:

    ~TestApp()
    {
        SAFE_CLEAR(text_render)
    }

private:

    bool InitUBO()
    {
        const VkExtent2D extent=sc_render_target->GetExtent();

        cam.width=extent.width;
        cam.height=extent.height;

        cam.RefreshCameraInfo();

        ubo_camera_info=db->CreateUBO(sizeof(CameraInfo),&cam.info);

        if(!ubo_camera_info)
            return(false);

        return(true);
    }

    bool InitTextRenderable()
    {
        UTF16String str;

        LoadStringFromTextFile(str,OS_TEXT("res/text/DaoDeBible.txt"));

        text_render_obj=text_render->CreateRenderable();
        if(!text_render_obj)
            return(false);

        render_instance=text_render->CreateRenderableInstance(text_render_obj,str);
        if(!render_instance)
            return(false);

        return(true);
    }

public:

    bool Init()
    {
        if(!VulkanApplicationFramework::Init(SCREEN_WIDTH,SCREEN_HEIGHT))
            return(false);

        if(!InitUBO())
            return(false);

        text_render=CreateTextRender(device,device_render_pass,ubo_camera_info);
        if(!text_render)
            return(false);

        if(!InitTextRenderable())
            return(false);

        BuildCommandBuffer(render_instance);

        return(true);
    }

    void Resize(int w,int h)override
    {
        cam.width=w;
        cam.height=h;
        
        cam.RefreshCameraInfo();

        ubo_camera_info->Write(&cam.info);
        
        BuildCommandBuffer(render_instance);
    }
};//class TestApp:public VulkanApplicationFramework

int main(int,char **)
{
    TestApp app;

    if(!app.Init())
        return(-1);

    while(app.Run());

    return 0;
}
