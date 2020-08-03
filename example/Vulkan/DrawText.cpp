// DrawTile
// 该示例使用TileData，演示多个tile图片在一张纹理上

#include<hgl/type/StringList.h>
#include<hgl/graph/TextureLoader.h>
#include<hgl/graph/TileData.h>
#include<hgl/graph/font/TileFont.h>
#include<hgl/graph/font/TextLayout.h>

#include"VulkanAppFramework.h"
#include<hgl/graph/vulkan/VKTexture.h>
#include<hgl/graph/vulkan/VKSampler.h>
#include<hgl/math/Math.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH =1280;
constexpr uint32_t SCREEN_HEIGHT=960;

constexpr uint CHAR_BITMAP_SIZE=16;         //字符尺寸
constexpr uint CHAR_BITMAP_BORDER=1;        //边界象素尺寸

class TestApp:public VulkanApplicationFramework
{
    Camera cam;

    Color4f color;
    
private:

    vulkan::Material *          material            =nullptr;
    vulkan::Sampler *           sampler             =nullptr;
    vulkan::MaterialInstance *  material_instance   =nullptr;
    vulkan::Renderable *        render_obj          =nullptr;
    vulkan::Buffer *            ubo_world_matrix    =nullptr;
    vulkan::Buffer *            ubo_color           =nullptr;

    vulkan::Pipeline *          pipeline            =nullptr;

private:

    TileFont *                  tile_font;
    TextLayout                  tl_engine;                                      ///<文本排版引擎

    RenderableCreater *         text_rc             =nullptr;

public:

    ~TestApp()
    {
        SAFE_CLEAR(text_rc);
        SAFE_CLEAR(tile_font);
    }

private:

    bool InitTileFont()
    {
        Font fnt(OS_TEXT("微软雅黑"),0,CHAR_BITMAP_SIZE);

        tile_font=device->CreateTileFont(fnt);
        return(true);
    }

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
        pipeline_creater->Set(PRIM_RECTANGLES);

        pipeline=pipeline_creater->Create();

        db->Add(pipeline);
        return pipeline;
    }

    bool InitTextRenderable()
    {
        CharLayoutAttr cla;
        TextLayoutAttributes tla;

        cla.CharColor=Color4f(COLOR::White);
        cla.BackgroundColor=Color4f(COLOR::Black);

        tla.char_layout_attr=&cla;

        text_rc=new RenderableCreater(db,material);

        tl_engine.Set(tile_font->GetFontSource());
        tl_engine.Set(text_rc);
        tl_engine.Set(&tla);
        tl_engine.SetTextDirection(0);
        tl_engine.Set(TextAlign::Left);
        tl_engine.SetMaxWidth(0);
        tl_engine.SetMaxHeight(0);

        if(!tl_engine.Init())
            return(false);

        UTF16String str;
        LoadStringFromTextFile(str,OS_TEXT("res/text/DaoDeBible.txt"));

        if(tl_engine.SimpleLayout(tile_font,str)>0)
        {
            render_obj=text_rc->Finish();
            return(true);
        }

        return(false);
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

        if(!InitTextRenderable())
            return(false);
            
        BuildCommandBuffer(pipeline,material_instance,render_obj);

        return(true);
    }

    void Resize(int w,int h)override
    {
        cam.width=w;
        cam.height=h;

        cam.Refresh();

        ubo_world_matrix->Write(&cam.matrix);

        BuildCommandBuffer(pipeline,material_instance,render_obj);
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
