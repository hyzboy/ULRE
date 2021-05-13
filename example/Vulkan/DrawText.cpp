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

constexpr uint CHAR_BITMAP_SIZE=12;         //字符尺寸

class TestApp:public VulkanApplicationFramework
{
    Camera cam;

    Color4f color;
    
private:

    Sampler *           sampler             =nullptr;
    MaterialInstance *  material_instance   =nullptr;
    GPUBuffer *         ubo_camera_info    =nullptr;
    GPUBuffer *         ubo_color           =nullptr;

    Pipeline *          pipeline            =nullptr;

private:

    FontSource *        eng_fs              =nullptr;
    FontSource *        chs_fs              =nullptr;
    FontSourceMulti *   font_source         =nullptr;

    TileFont *          tile_font           =nullptr;
    TextLayout          tl_engine;                                      ///<文本排版引擎

    TextRenderable *    text_render_obj     =nullptr;
    RenderableInstance *render_instance     =nullptr;

public:

    ~TestApp()
    {
        SAFE_CLEAR(tile_font);
    }

private:

    bool InitMaterial()
    {
        material_instance=db->CreateMaterialInstance(OS_TEXT("res/material/LumTextureRect2D"));
        if(!material_instance)return(false);

        pipeline=CreatePipeline(material_instance,InlinePipeline::Solid2D,Prim::Rectangles);
        if(!pipeline)return(false);

        sampler=db->CreateSampler();

        material_instance->BindSampler("lum_texture",tile_font->GetTexture(),sampler);
        material_instance->BindUBO("camera",ubo_camera_info);
        material_instance->BindUBO("color_material",ubo_color);
        material_instance->Update();

        return(true);
    }

    bool InitUBO()
    {
        const VkExtent2D extent=sc_render_target->GetExtent();

        cam.width=extent.width;
        cam.height=extent.height;

        cam.Refresh();

        ubo_camera_info=db->CreateUBO(sizeof(CameraInfo),&cam.info);

        if(!ubo_camera_info)
            return(false);

        color.One();

        ubo_color=db->CreateUBO(sizeof(Color4f),&color);

        if(!ubo_color)
            return(false);

        return(true);
    }

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
        tl_engine.SetTLA(&tla);
        tl_engine.SetTextDirection(0);
        tl_engine.SetAlign(TextAlign::Left);
        tl_engine.SetMaxWidth(0);
        tl_engine.SetMaxHeight(0);

        return tl_engine.Init();
    }

    bool InitTextRenderable()
    {
        UTF16String str;

        LoadStringFromTextFile(str,OS_TEXT("README.md"));

        text_render_obj=db->CreateTextRenderable(material_instance->GetMaterial());

        if(tl_engine.SimpleLayout(text_render_obj,tile_font,str)<=0)return(false);

        render_instance=db->CreateRenderableInstance(text_render_obj,material_instance,pipeline);

        return(render_instance);
    }

public:

    bool Init()
    {
        if(!VulkanApplicationFramework::Init(SCREEN_WIDTH,SCREEN_HEIGHT))
            return(false);
            
        if(!InitTileFont())
            return(false);

        if(!InitUBO())
            return(false);

        if(!InitMaterial())
            return(false);

        if(!InitTextLayoutEngine())
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

        cam.Refresh();

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
