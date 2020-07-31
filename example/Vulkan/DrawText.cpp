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

/**
 * 文本绘制技术流程：
 *
 * 1.由TextLayout模块排版所有的字符，并向FontSource获取所有字符的Bitmap。生成文本可渲染对象的vertex position数据
 * 2.由TextLayout向TileData提交需要渲染的所有字符的Bitmap，并得到每个Bitmap对应的UV数据。生成文本可渲染对象的uv数据
 */

constexpr uint32_t SCREEN_WIDTH =1280;
constexpr uint32_t SCREEN_HEIGHT=960;

constexpr uint CHAR_BITMAP_SIZE=16;         //字符尺寸
constexpr uint CHAR_BITMAP_BORDER=1;        //边界象素尺寸

class TestApp:public VulkanApplicationFramework
{
    Camera cam;
    
private:

    vulkan::Material *          material            =nullptr;
    vulkan::Sampler *           sampler             =nullptr;
    vulkan::MaterialInstance *  material_instance   =nullptr;
    vulkan::Renderable *        render_obj          =nullptr;
    vulkan::Buffer *            ubo_world_matrix    =nullptr;

    vulkan::Pipeline *          pipeline            =nullptr;

private:

    TileFont *                  tile_font;
    TextLayout                  tl_engine;                                      ///<文本排版引擎

    RenderableCreater *         text_rc             =nullptr;
    Renderable *                text_renderable     =nullptr;                   ///<文本渲染对象列表

public:

    ~TestApp()
    {
        SAFE_CLEAR(text_renderable);
        SAFE_CLEAR(text_rc);
        SAFE_CLEAR(tile_font);
    }

private:

    bool InitTileFont()
    {
        Font fnt(OS_TEXT("Consolas"),0,CHAR_BITMAP_SIZE);

        tile_font=device->CreateTileFont(fnt);
        return(true);
    }

    bool InitTextRenderable()
    {
        CharAttributes ca;
        TextLayoutAttributes tla;

        ca.CharColor=Color4f(COLOR::White);
        ca.BackgroundColor=Color4f(COLOR::Black);

        tla.char_attributes=&ca;

        text_rc=new RenderableCreater(db,material);

        tl_engine.Set(text_rc);
        tl_engine.Set(&tla);
        tl_engine.SetTextDirection(0);
        tl_engine.Set(TextAlign::Left);
        tl_engine.SetMaxWidth(0);
        tl_engine.SetMaxHeight(0);

        if(!tl_engine.Init())
            return(false);

        UTF16String str=U16_TEXT("道可道，非常道。名可名，非常名。无名天地之始。有名万物之母。故常无欲以观其妙。常有欲以观其徼。此两者同出而异名，同谓之玄。玄之又玄，众妙之门。");

        tl_engine.PlaneLayout(tile_font,0,str);
    }

    bool InitMaterial()
    {
        material=shader_manage->CreateMaterial( OS_TEXT("res/shader/DrawRect2D.vert"),
                                                OS_TEXT("res/shader/DrawRect2D.geom"),
                                                OS_TEXT("res/shader/FlatTexture.frag"));
        if(!material)
            return(false);

        material_instance=material->CreateInstance();

        sampler=db->CreateSampler();

        material_instance->BindSampler("tex",tile_data->GetTexture(),sampler);
        material_instance->BindUBO("world",ubo_world_matrix);
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

        return(true);
    }

    void InitVBO()
    {
        const int tile_count=tile_list.GetCount();

        render_obj=material->CreateRenderable(tile_count);

        vertex_buffer   =db->CreateVAB(VAF_VEC4,tile_count,vertex_data);
        tex_coord_buffer=db->CreateVAB(VAF_VEC4,tile_count,tex_coord_data);

        render_obj->Set("Vertex",vertex_buffer);
        render_obj->Set("TexCoord",tex_coord_buffer);

        db->Add(render_obj);
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

public:

    bool Init()
    {
        if(!VulkanApplicationFramework::Init(SCREEN_WIDTH,SCREEN_HEIGHT))
            return(false);

        if(!InitTileData())
            return(false);

        if(!InitTileFont())
            return(false);

        if(!InitUBO())
            return(false);

        if(!InitMaterial())
            return(false);

        InitVBO();

        if(!InitPipeline())
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
