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

namespace hgl
{
    namespace graph
    {
        TextRender::TextRender(VulkanDevice *dev,TileFont *tf,FontSource *fs)
        {
            device=dev;

            db=new RenderResource(device);         //独立的资源管理器，不和整体共用
            tl_engine=new TextLayout();
            font_source=fs;
            
            material            =nullptr;
            material_instance   =nullptr;
            sampler             =nullptr;
            pipeline            =nullptr;
            tile_font           =tf;

            color.Set(1,1,1,1);
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

        bool TextRender::InitTextLayoutEngine()
        {
            CharLayoutAttr cla;
            TextLayoutAttribute tla;

            cla.CharColor=GetColor4f(COLOR::White,1.0);
            cla.BackgroundColor.Zero();

            tla.char_layout_attr=&cla;
            tla.line_gap=0.1f;

            tl_engine->SetFont(tile_font->GetFontSource());
            tl_engine->SetTLA(&tla);
            tl_engine->SetTextDirection(0);
            tl_engine->SetAlign(TextAlign::Left);
            tl_engine->SetMaxWidth(0);
            tl_engine->SetMaxHeight(0);

            return tl_engine->Init();
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

                material_instance=db->CreateMaterialInstance(material,&vil_config,color,sizeof(color));
                if(!material_instance)return(false);
            }

            pipeline=rp->CreatePipeline(material_instance,InlinePipeline::Solid2D);
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
            if(!InitTextLayoutEngine())
                return(false);

            if(!InitMaterial(rp))
                return(false);

            return(true);
        }

        TextPrimitive *TextRender::CreatePrimitive(int limit)
        {   
            TextPrimitive *tr=new TextPrimitive(device,material_instance->GetVIL(),limit);

            tr_sets.Add(tr);

            return tr;
        }

        TextPrimitive *TextRender::CreatePrimitive(const U16String &str)
        {
            TextPrimitive *tr=CreatePrimitive();

            if(tl_engine->SimpleLayout(tr,tile_font,str)<=0)
                return(tr);

            return tr;
        }

        bool TextRender::Layout(TextPrimitive *tr,const U16String &str)
        {
            if(!tr)
                return(false);

            if(tl_engine->SimpleLayout(tr,tile_font,str)<=0)
                return(false);

            return true;
        }

        Mesh *TextRender::CreateMesh(TextPrimitive *text_primitive)
        {
            return db->CreateMesh(text_primitive,material_instance,pipeline);
        }

        void TextRender::Release(TextPrimitive *tr)
        {
            if(!tr)return;

            if(!tr_sets.Delete(tr))return;

            tile_font->Unregistry(tr->GetCharsSets());

            delete tr;
        }

        FontSource *CreateCJKFontSource(const os_char *cf,const os_char *lf,const uint32_t size)
        {
            Font eng_fnt(lf,0,size);
            Font chs_fnt(cf,0,size);

            FontSource *eng_fs=AcquireFontSource(eng_fnt);
            FontSource *chs_fs=AcquireFontSource(chs_fnt);

            FontSourceMulti *font_source=new FontSourceMulti(eng_fs);

            font_source->AddCJK(chs_fs);

            return font_source;
        }

        FontSource *AcquireFontSource(const os_char *name,const uint32_t size)
        {
            Font fnt(name,0,size);

            return AcquireFontSource(fnt);
        }

        TextRender *RenderFramework::CreateTextRender(FontSource *font_source,const int limit)
        {
            if(!font_source)
                return(nullptr);

            TileFont *tile_font=CreateTileFont(font_source,limit);

            RenderPass *rp=GetDefaultRenderPass();

            if(!rp)
                return(nullptr);

            TextRender *text_render=new TextRender(GetDevice(),tile_font,font_source);

            if(!text_render)
            {
                delete tile_font;
                return(nullptr);
            }

            if(!text_render->Init(rp))
            {
                delete text_render;
                return(nullptr);
            }

            return text_render;
        }
    }//namespace graph
}//namespace hgl

