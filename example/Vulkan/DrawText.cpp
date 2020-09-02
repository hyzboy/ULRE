// DrawTile
// 该示例使用TileData，演示多个tile图片在一张纹理上

#include<hgl/type/StringList.h>
#include<hgl/graph/TextureLoader.h>
#include<hgl/graph/TileData.h>
#include<hgl/graph/font/TileFont.h>
#include<hgl/graph/font/TextLayout.h>
#include<hgl/graph/font/TextRenderable.h>

#include"VulkanAppFramework.h"
#include<hgl/graph/vulkan/VKTexture.h>
#include<hgl/graph/vulkan/VKSampler.h>
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

    vulkan::Material *          material            =nullptr;
    vulkan::Sampler *           sampler             =nullptr;
    vulkan::MaterialInstance *  material_instance   =nullptr;
    vulkan::Buffer *            ubo_world_matrix    =nullptr;
    vulkan::Buffer *            ubo_color           =nullptr;

    vulkan::Pipeline *          pipeline            =nullptr;

private:

    FontSource *                eng_fs              =nullptr;
    FontSource *                chs_fs              =nullptr;
    FontSourceMulti *           font_source         =nullptr;

    TileFont *                  tile_font           =nullptr;
    TextLayout                  tl_engine;                                      ///<文本排版引擎

    TextRenderable *            text_render_obj     =nullptr;

public:

    ~TestApp()
    {
        SAFE_CLEAR(tile_font);
    }

private:

    bool InitMaterial()
    {
        material=shader_manage->CreateMaterial( OS_TEXT("res/shader/DrawRect2D.vert"),
                                                OS_TEXT("res/shader/DrawRect2D.geom"),
                                                OS_TEXT("res/shader/FlatLumTexture.frag"));
        if(!material)
            return(false);

        material_instance=material->CreateInstance();

        sampler=db->CreateSampler();

        material_instance->BindSampler("tex",tile_font->GetTexture(),sampler);
        material_instance->BindUBO("world",ubo_world_matrix);
        material_instance->BindUBO("color_material",ubo_color);
        material_instance->Update();

        db->Add(material);
        db->Add(material_instance);
        return(true);
    }

    bool InitUBO()
    {
        const VkExtent2D extent=sc_render_target->GetExtent();

        cam.width=extent.width;
        cam.height=extent.height;

        cam.Refresh();

        ubo_world_matrix=db->CreateUBO(sizeof(WorldMatrix),&cam.matrix);

        if(!ubo_world_matrix)
            return(false);

        color.One();

        ubo_color=db->CreateUBO(sizeof(Color4f),&color);

        if(!ubo_color)
            return(false);

        return(true);
    }

    bool InitPipeline()
    {
        AutoDelete<vulkan::PipelineCreater>
        pipeline_creater=new vulkan::PipelineCreater(device,material,sc_render_target);
        pipeline_creater->CloseCullFace();
        pipeline_creater->Set(Prim::Rectangles);

        pipeline=pipeline_creater->Create();

        db->Add(pipeline);
        return pipeline;
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

        tl_engine.Set(tile_font->GetFontSource());
        tl_engine.Set(&tla);
        tl_engine.SetTextDirection(0);
        tl_engine.Set(TextAlign::Left);
        tl_engine.SetMaxWidth(0);
        tl_engine.SetMaxHeight(0);

        return tl_engine.Init();
    }

    bool InitTextRenderable()
    {
        UTF16String str;

        LoadStringFromTextFile(str,OS_TEXT("README.md"));

        text_render_obj=db->CreateTextRenderable(material);

        return(tl_engine.SimpleLayout(text_render_obj,tile_font,str)>0);
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

        if(!InitPipeline())
            return(false);

        if(!InitTextLayoutEngine())
            return(false);

        if(!InitTextRenderable())
            return(false);
            
        BuildCommandBuffer(pipeline,material_instance,text_render_obj);

        return(true);
    }

    void Resize(int w,int h)override
    {
        cam.width=w;
        cam.height=h;

        cam.Refresh();

        ubo_world_matrix->Write(&cam.matrix);

        BuildCommandBuffer(pipeline,material_instance,text_render_obj);
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
