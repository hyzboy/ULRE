﻿// DrawTile
// 该示例使用TileData，演示多个tile图片在一张纹理上

#include<hgl/type/StringList.h>
#include<hgl/graph/Bitmap2DLoader.h>
#include<hgl/graph/TileData.h>

#include"VulkanAppFramework.h"
#include<hgl/graph/VKTexture.h>
#include<hgl/graph/VKSampler.h>
#include<hgl/math/Math.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH =1024;
constexpr uint32_t SCREEN_HEIGHT=512;

constexpr float BORDER=2;
constexpr uint TILE_COLS=10;

struct TileBitmap
{
    BitmapData *bmp;
    TileObject *to;
};

class TestApp:public VulkanApplicationFramework
{    
    ObjectList<TileBitmap> tile_list;

    TileData *tile_data=nullptr;

    float *position_data=nullptr;
    float *tex_coord_data=nullptr;

    VkFormat tile_texture_format=PF_UNDEFINED;

private:

    Sampler *           sampler             =nullptr;
    MaterialInstance *  material_instance   =nullptr;
    Primitive *         primitive           =nullptr;
    Renderable *        render_obj          =nullptr;

    Pipeline *          pipeline            =nullptr;

public:

    ~TestApp()
    {
        SAFE_CLEAR(tile_data);
    }

private:

    int LoadIcons()
    {
        const OSString icon_path=OS_TEXT("res/image/icon/freepik/");

        UTF8StringList sl;

        const int count=LoadStringListFromTextFile(sl,icon_path+OS_TEXT("list.txt"));

        Bitmap2DLoader loader;
        
        int result=0;

        const VulkanFormat *vf=GetVulkanFormat(sl[0].c_str());

        if(!vf)
        {
            LOG_ERROR("can't support the format: "+sl[0]);
            return(-1);
        }

        tile_texture_format=vf->format;

        for(int i=1;i<count;i++)
        {
            if(loader.Load(icon_path+ToOSString(sl[i])+OS_TEXT(".Tex2D")))
            {
                TileBitmap *tb=new TileBitmap;

                tb->bmp =loader.GetBitmap();
                tb->to  =nullptr;

                tile_list.Add(tb);
                ++result;
            }
        }

        return result;
    }

    bool InitTileTexture()
    {
        tile_data=device->CreateTileData(   tile_texture_format,    //纹理格式，不支持实时转换，所以提交的数据格式必须与此一致
                                            512,512,                //TILE大小
                                            tile_list.GetCount());  //TILE需求数量

        if(!tile_data)
            return(false);

        const int count=tile_list.GetCount();
        TileBitmap **tb=tile_list.GetData();

        position_data=new float[count*4];
        tex_coord_data=new float[count*4];

        float *vp=position_data;
        float *tp=tex_coord_data;

        int col=0;
        int row=0;

        float size      =SCREEN_WIDTH/TILE_COLS;
        float view_size =size-BORDER*2;
        float left      =0;
        float top       =0;

        tile_data->BeginCommit();

        for(int i=0;i<count;i++)
        {
            (*tb)->to=tile_data->Commit((*tb)->bmp);        //添加一个tile图片

            vp=WriteRect(vp,left+BORDER,                    //产生绘制顶点信息
                            top +BORDER,
                            view_size,
                            view_size);

            tp=WriteRect(tp,(*tb)->to->uv_float);           //产生绘制纹理坐标信息

            ++col;
            if(col==TILE_COLS)
            {
                left=0;
                top+=size;
                ++row;
                col=0;
            }
            else
            {
                left+=size;
            }

            ++tb;
        }

        tile_data->EndCommit();

        return(true);
    }

    bool InitMaterial()
    {
        material_instance=db->CreateMaterialInstance(OS_TEXT("res/material/TextureRect2D"));
        if(!material_instance)
            return(false);

        BindCameraUBO(material_instance);

        pipeline=CreatePipeline(material_instance,InlinePipeline::Solid2D,Prim::SolidRectangles);

        sampler=db->CreateSampler();        

        if(!material_instance->BindImageSampler(DescriptorSetType::Value,"tex",tile_data->GetTexture(),sampler))return(false);

        return(true);
    }

    bool InitVBO()
    {
        const uint tile_count=tile_list.GetCount();

        primitive=db->CreatePrimitive(tile_count);
        if(!primitive)return(false);

        primitive->Set(VAN::Position,db->CreateVBO(VF_V4F,tile_count,position_data));
        primitive->Set(VAN::TexCoord,db->CreateVBO(VF_V4F,tile_count,tex_coord_data));

        render_obj=db->CreateRenderable(primitive,material_instance,pipeline);

        return(render_obj);
    }

public:

    bool Init()
    {
        if(LoadIcons()<=0)
            return(false);

        if(!VulkanApplicationFramework::Init(SCREEN_WIDTH,SCREEN_HEIGHT))
            return(false);

        if(!InitTileTexture())
            return(false);

        if(!InitMaterial())
            return(false);

        if(!InitVBO())
            return(false);
            
        BuildCommandBuffer(render_obj);

        return(true);
    }

    void Resize(int w,int h)override
    {
        VulkanApplicationFramework::Resize(w,h);
        
        BuildCommandBuffer(render_obj);
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
