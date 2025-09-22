#include<hgl/graph/font/TextRender.h>
#include<hgl/graph/font/TextPrimitive.h>
#include<hgl/graph/font/TileFont.h>
#include<hgl/graph/font/TextLayout.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKVertexInputConfig.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>
#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/MeshManager.h>
#include<hgl/color/Color.h>
#include"TextLayoutEngine.h"

namespace hgl::graph
{
    using namespace layout;

    namespace
    {
        void SetDrawStyle(TextDrawStyle &tda,const ParagraphStyle *t,const float origin_char_height)
        {
            hgl_cpy(tda.para_style,*t);

            hgl_zero(tda.start_position);

            tda.char_height     =std::ceil(origin_char_height);
            tda.space_size      =std::ceil(origin_char_height*tda.space_size);
            tda.full_space_size =std::ceil(origin_char_height*tda.full_space_size);
            tda.tab_size        =std::ceil(origin_char_height*tda.tab_size);
            tda.char_gap        =std::ceil(origin_char_height*tda.char_gap);
            tda.line_gap        =std::ceil(origin_char_height*tda.line_gap);
            tda.line_height     =std::ceil(origin_char_height+tda.line_gap);
        }
    }//namespace

    TextRender::TextRender(RenderFramework *rf,TileFont *tf)
    {
        device=rf->GetDevice();

        mesh_manager=rf->GetMeshManager();
        mtl_manager=rf->GetMaterialManager();
        tl_engine=new layout::TextLayout(tf);
            
        mtl_fs      =nullptr;
        sampler     =nullptr;
        pipeline    =nullptr;
        tile_font   =tf;

        fixed_style.CharColor=GetColor4ub(COLOR::White);

        SetDrawStyle(text_draw_style,&para_style,(float)tile_font->GetFontSource()->GetCharHeight());

        mi_fs=nullptr;
    }

    void TextRender::SetFixedStyle(const layout::CharStyle &cs)
    {
        if(!hgl_cmp(fixed_style,cs))
            return;

        fixed_style=cs;
        mi_fs->WriteMIData(fixed_style);
    }

    void TextRender::SetParagraphStyle(const layout::ParagraphStyle *ps)
    {
        if(!ps)
            return;

        if(hgl_cmp(para_style,*ps))
            return;

        hgl_cpy(para_style,*ps);
        SetDrawStyle(text_draw_style,&para_style,(float)tile_font->GetFontSource()->GetCharHeight());
    }

    TextRender::~TextRender()
    {
        for(TextPrimitive *tr:tr_sets)
            delete tr;

        tr_sets.Clear();
           
        SAFE_CLEAR(tl_engine);
        SAFE_CLEAR(tile_font);
        // render resource removed
    }

    bool TextRender::InitMaterial(RenderPass *rp)
    {
        mtl::Text2DMaterialCreateConfig mtl_cfg;

        mtl::MaterialCreateInfo *mci=mtl::CreateText2D(device->GetDevAttr(),&mtl_cfg);

        if (!mci)return(false);

        mtl_fs=mtl_manager->CreateMaterial("Text2D",mci);

        //文本渲染Position坐标全部是使用整数，这里强制要求Position输入流使用RGBA16I格式
        {
            VILConfig vil_config;

            vil_config.Add("Position",VF_V4I16);

            mi_fs=mtl_manager->CreateMaterialInstance(mtl_fs,&vil_config,&fixed_style,sizeof(fixed_style));
            if(!mi_fs)return(false);
        }

        pipeline=rp->CreatePipeline(mi_fs,InlinePipeline::Solid2D);
        if(!pipeline)return(false);

        if(!mtl_fs->BindTextureSampler( DescriptorSetType::PerMaterial,
                                        mtl::SamplerName::Text,
                                        tile_font->GetTexture(),
                                        sampler))
            return(false);

        return(true);
    }

    bool TextRender::Init(RenderPass *rp,Sampler *text_sampler)
    {
        sampler=text_sampler;

        if(!InitMaterial(rp))
            return(false);

        return(true);
    }

    TextPrimitive *TextRender::Begin(const TextPrimitiveType &tpt,int limit)
    {   
        TextPrimitive *tr=new TextPrimitive(device,mi_fs->GetVIL(),limit);

        tr_sets.Add(tr);

        tl_engine->Begin(tr,limit);

        return tr;
    }

    bool TextRender::Layout(const layout::TEXT_COORD_VEC &start_pos,const U16StringView &str)
    {
        TextDrawStyle tds=text_draw_style;

        tds.start_position=start_pos;

        return tl_engine->AddString(str,tds);
    }

    void TextRender::End()
    {
        tl_engine->End();
    }

    TextPrimitive *TextRender::CreatePrimitive(const TextPrimitiveType &tpt,const U16StringView&str)
    {
        TextPrimitive *tr=Begin(tpt,str.Length());

        if(!tr)
            return(nullptr);

        if(!SimpleLayout(tr,str))
        {
            delete tr;
            return(nullptr);
        }

        return tr;
    }

    bool TextRender::SimpleLayout(TextPrimitive *tr,const U16StringView&str)
    {
        if(!tr)
            return(false);

        if(!tl_engine->Begin(tr,str.Length()))
            return(false);

        const bool result=tl_engine->AddString(str,text_draw_style);

        tl_engine->End();

        return(result);
    }

    Mesh *TextRender::CreateMesh(TextPrimitive *text_primitive)
    {
        if(mesh_manager)
            return mesh_manager->CreateMesh(text_primitive,mi_fs,pipeline);

        return(nullptr);
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

        TextRender *text_render=new TextRender(this,tile_font);

        if(!text_render)
        {
            delete tile_font;
            return(nullptr);
        }

        if(!text_render->Init(rp,sampler_manager->CreateSampler()))
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
}//namespace hgl::graph
