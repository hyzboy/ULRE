#include<hgl/graph/font/TextRender.h>
#include<hgl/graph/font/TextPrimitive.h>
#include<hgl/graph/font/TileFont.h>
#include<hgl/graph/font/TextLayout.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKVertexInputConfig.h>
#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>
#include<hgl/graph/RenderFramework.h>
#include<hgl/color/Color.h>
#include"TextLayoutEngine.h"

namespace hgl::graph
{
    using namespace layout;

    TextRender::TextRender(VulkanDevice *dev,TileFont *tf)
    {
        device=dev;

        db=new RenderResource(device);         //独立的资源管理器，不和整体共用
        tl_engine=new layout::TextLayout(tf->GetFontSource());
            
        material            =nullptr;
        sampler             =nullptr;
        pipeline            =nullptr;
        tile_font           =tf;

        default_char_style_material.char_style.CharColor.Set(255,255,255,255);
        default_char_style_material.mi=nullptr;
        cur_char_style_mi=nullptr;
    }

    TextRender::~TextRender()
    {
        for(TextPrimitive *tr:tr_sets)
        {
            tile_font->Unregistry(tr->GetCharsSets());
            delete tr;
        }

        tr_sets.Clear();
           
        SAFE_CLEAR(tl_engine);
        SAFE_CLEAR(tile_font);
        SAFE_CLEAR(db);
    }

    bool TextRender::InitMaterial(RenderPass *rp)
    {
        mtl::Text2DMaterialCreateConfig mtl_cfg;

        mtl::MaterialCreateInfo *mci=mtl::CreateText2D(device->GetDevAttr(),&mtl_cfg);

        if (!mci)return(false);

        material=db->CreateMaterial("Text2D",mci);

        //文本渲染Position坐标全部是使用整数，这里强制要求Position输入流使用RGBA16I格式
        {
            VILConfig vil_config;

            vil_config.Add("Position",VF_V4I16);

            default_char_style_material.mi=db->CreateMaterialInstance(material,&vil_config,&default_char_style_material.char_style);
            if(!default_char_style_material.mi)return(false);

            cur_char_style_mi=default_char_style_material.mi; //设置当前材质实例为默认材质实例
        }

        pipeline=rp->CreatePipeline(default_char_style_material.mi,InlinePipeline::Solid2D);
        if(!pipeline)return(false);

        sampler=db->CreateSampler();

        if(!material->BindImageSampler(DescriptorSetType::PerMaterial,
                                        mtl::SamplerName::Text,
                                        tile_font->GetTexture(),
                                        sampler))
            return(false);

        return(true);
    }

    bool TextRender::Init(RenderPass *rp)
    {
        if(!InitMaterial(rp))
            return(false);

        return(true);
    }

    TextPrimitive *TextRender::CreatePrimitive(const TextPrimitiveType &tpt,int limit)
    {   
        TextPrimitive *tr=new TextPrimitive(device,default_char_style_material.mi->GetVIL(),limit);

        tr_sets.Add(tr);

        return tr;
    }

    TextPrimitive *TextRender::CreatePrimitive(const TextPrimitiveType &tpt,const U16String &str)
    {
        TextPrimitive *tr=CreatePrimitive(tpt,str.Length());

        if(!tr)
            return(nullptr);

        if(!Layout(tr,str))
        {
            delete tr;
            return(nullptr);
        }

        return tr;
    }

    bool TextRender::Layout(TextPrimitive *tr,const U16String &str)
    {
        if(!tr)
            return(false);

        if(!tl_engine->Begin(tr,tile_font,str.Length()))
            return(false);

        if(tl_engine->SimpleLayout(tr,tile_font,str)<=0)
            return(false);

        return true;
    }

    Mesh *TextRender::CreateMesh(TextPrimitive *text_primitive)
    {
        return db->CreateMesh(text_primitive,cur_char_style_mi,pipeline);
    }

    void TextRender::Release(TextPrimitive *tr)
    {
        if(!tr)return;

        if(!tr_sets.Delete(tr))return;

        tile_font->Unregistry(tr->GetCharsSets());

        delete tr;
    }

    TextRender *RenderFramework::CreateTextRender(FontSource *font_source,const int limit)
    {
        if(!font_source)
            return(nullptr);

        TileFont *tile_font=CreateTileFont(font_source,limit);

        RenderPass *rp=GetDefaultRenderPass();

        if(!rp)
            return(nullptr);

        TextRender *text_render=new TextRender(GetDevice(),tile_font);

        if(!text_render)
        {
            delete tile_font;
            return(nullptr);
        }

        if(!text_render->Init(rp))
        {
            delete tile_font;
            delete text_render;
            return(nullptr);
        }

        return text_render;
    }

    TextRender *RenderFramework::CreateTextRender(const OSString &latin_font,const OSString &cjk_font,const int font_size,const int limit_count)
    {
        FontSource *fs=CreateCJKFontSource(latin_font,cjk_font,font_size);

        TextRender *tr=CreateTextRender(fs,limit_count);

        if(tr)
            return tr;

        delete fs;
        return(nullptr);
    }

    bool TextRender::RegistryStyle(const AnsiString &style_name,const CharStyle &cds)
    {
        if(style_name.IsEmpty())
            return(false);

        if(char_style_materials.ContainsKey(style_name))
            return(false);

        CharStyleMaterial csm;

        csm.char_style=cds;
        csm.mi=db->CreateMaterialInstance(material,default_char_style_material.mi->GetVIL(),&csm.char_style);

        char_style_materials.Add(style_name,csm);
        return(true);
    }

    void TextRender::SetStyle(const AnsiString &style_name)
    {
        if(!char_style_materials.ContainsKey(style_name))
            return;

        CharStyleMaterial csm;

        char_style_materials.Get(style_name,csm);

        cur_char_style_mi=csm.mi;
    }

    void TextRender::SetLayout(const ParagraphStyle *tla)
    {
        if(!tla)
            return;

        tl_engine->Set(tla);
    }
}//namespace hgl::graph
