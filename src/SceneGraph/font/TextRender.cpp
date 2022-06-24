#include<hgl/graph/font/TextRender.h>
#include<hgl/graph/font/TextPrimitive.h>
#include<hgl/graph/font/TileFont.h>
#include<hgl/graph/font/TextLayout.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/type/Color.h>

namespace hgl
{
    namespace graph
    {
        TextRender::TextRender(GPUDevice *dev,FontSource *fs)
        {
            device=dev;
            db=new RenderResource(device);
            tl_engine=new TextLayout();
            font_source=fs;
            
            material            =nullptr;
            material_instance   =nullptr;
            sampler             =nullptr;
            pipeline            =nullptr;
            tile_font           =nullptr;    
            ubo_color           =nullptr;
        }

        TextRender::~TextRender()
        {
            for(TextPrimitive *tr:tr_sets)
            {
                tile_font->Unregistry(tr->GetCharsSets().GetList());
                delete tr;
            }

            tr_sets.Clear();
            
            SAFE_CLEAR(tl_engine);
            SAFE_CLEAR(tile_font);
            SAFE_CLEAR(db);
        }

        bool TextRender::InitTileFont(int limit)
        {
            tile_font=device->CreateTileFont(font_source,limit);
            return(true);
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
            tl_engine->Set(&tla);
            tl_engine->SetTextDirection(0);
            tl_engine->SetAlign(TextAlign::Left);
            tl_engine->SetMaxWidth(0);
            tl_engine->SetMaxHeight(0);

            return tl_engine->Init();
        }

        bool TextRender::InitUBO()
        {
            color.One();

            ubo_color=db->CreateUBO(sizeof(Color4f),&color);

            if(!ubo_color)
                return(false);

            return(true);
        }

        bool TextRender::InitMaterial(RenderPass *rp,GPUBuffer *ubo_camera_info)
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

        bool TextRender::Init(RenderPass *rp,GPUBuffer *ubo_camera_info,int limit)
        {
            if(!InitTileFont(limit))
                return(false);

            if(!InitTextLayoutEngine())
                return(false);

            if(!InitUBO())
                return(false);

            if(!InitMaterial(rp,ubo_camera_info))
                return(false);

            return(true);
        }

        TextPrimitive *TextRender::CreatePrimitive()
        {   
            TextPrimitive *tr=new TextPrimitive(device,material);

            tr_sets.Add(tr);

            return tr;
        }

        TextPrimitive *TextRender::CreatePrimitive(const UTF16String &str)
        {
            TextPrimitive *tr=CreatePrimitive();

            if(tl_engine->SimpleLayout(tr,tile_font,str)<=0)
                return(tr);

            return tr;
        }

        bool TextRender::Layout(TextPrimitive *tr,const UTF16String &str)
        {
            if(!tr)
                return(false);

            if(tl_engine->SimpleLayout(tr,tile_font,str)<=0)
                return(false);

            return true;
        }

        Renderable *TextRender::CreateRenderable(TextPrimitive *text_render_obj)
        {
            return db->CreateRenderable(text_render_obj,material_instance,pipeline);
        }

        void TextRender::Release(TextPrimitive *tr)
        {
            if(!tr)return;

            if(!tr_sets.Delete(tr))return;

            tile_font->Unregistry(tr->GetCharsSets().GetList());

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

        TextRender *CreateTextRender(GPUDevice *dev,FontSource *fs,RenderPass *rp,GPUBuffer *ubo_camera_info,int limit)
        {
            if(!dev||!rp||!ubo_camera_info)
                return(nullptr);

            TextRender *text_render=new TextRender(dev,fs);

            if(!text_render->Init(rp,ubo_camera_info,limit))
            {
                delete text_render;
                return(nullptr);
            }

            return text_render;
        }
    }//namespace graph
}//namespace hgl

