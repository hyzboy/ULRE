#include<hgl/graph/font/TextRender.h>
#include<hgl/graph/font/TextGeometry.h>
#include<hgl/graph/font/TileFont.h>
#include<hgl/graph/font/TextLayout.h>
#include<hgl/graph/font/TextLayoutEngine.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKVertexInputConfig.h>
#include<hgl/mtl/Material2DCreateConfig.h>
#include<hgl/framework/AppFramework.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/MaterialAssetRegistry.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/vk/VKMaterialTemplate.h>
#include<hgl/vk/pipeline/VKRenderTargetFormat.h>
#include<hgl/type/AlignUtil.h>
#include<hgl/vk/VKFormat.h>
#include<hgl/color/Color.h>

namespace hgl::graph
{
    using namespace layout;

    namespace
    {
        void SetDrawStyle(TextDrawStyle &tda,const ParagraphStyle *t,const float origin_char_height)
        {
            mem_copy(tda.para_style,*t);

            mem_zero(tda.start_position);

            tda.char_height     =std::ceil(origin_char_height);
            tda.space_size      =std::ceil(origin_char_height*tda.space_size);
            tda.full_space_size =std::ceil(origin_char_height*tda.full_space_size);
            tda.tab_size        =std::ceil(origin_char_height*tda.tab_size);
            tda.char_gap        =std::ceil(origin_char_height*tda.char_gap);
            tda.line_gap        =std::ceil(origin_char_height*tda.line_gap);
            tda.line_height     =std::ceil(origin_char_height+tda.line_gap);
        }
    }//namespace

    TextRender::TextRender(AppFramework *af,TileFont *tf)
        : TextRender(af ? af->GetGraphicsContext() : nullptr, tf)
    {
    }

    TextRender::TextRender(GraphicsContext *gc,TileFont *tf)
    {
        device = gc ? gc->GetDevice() : nullptr;

        primitive_manager = gc ? gc->GetPrimitiveManager() : nullptr;
        mtl_manager = gc ? gc->GetMaterialManager() : nullptr;
        material_registry = gc ? gc->GetMaterialAssetRegistry() : nullptr;
        tl_engine = new layout::TextLayout(tf);

        mtl_fs      =nullptr;
        sampler     =nullptr;
        tile_font   =tf;

        fixed_style.CharColor=GetColor4ub(COLOR::White);

        SetDrawStyle(text_draw_style,&para_style,(float)tile_font->GetFontSource()->GetCharHeight());

        fixed_vil = nullptr;
        fixed_style_handle = 0;
    }

    void TextRender::SetFixedStyle(const layout::CharStyle &cs)
    {
        if(!mem_compare(fixed_style,cs))
            return;

        fixed_style=cs;
        if (material_registry && fixed_style_handle != InvalidMaterialInstanceHandle)
            material_registry->WriteMIData(fixed_style_handle, &fixed_style, sizeof(fixed_style));
    }

    void TextRender::SetParagraphStyle(const layout::ParagraphStyle *ps)
    {
        if(!ps)
            return;

        if(mem_compare(para_style,*ps))
            return;

        mem_copy(para_style,*ps);
        SetDrawStyle(text_draw_style,&para_style,(float)tile_font->GetFontSource()->GetCharHeight());
    }

    TextRender::~TextRender()
    {
        for(TextGeometry *tr:text_geometry_set)
            delete tr;

        text_geometry_set.Clear();

        SAFE_CLEAR(tl_engine);
        SAFE_CLEAR(tile_font);

        if (material_registry && fixed_style_handle != InvalidMaterialInstanceHandle)
        {
            material_registry->ReleaseHandle(fixed_style_handle);
            fixed_style_handle = InvalidMaterialInstanceHandle;
        }

        // render resource removed
    }

    namespace
    {
        TileFont *CreateTileFont(GraphicsContext *gc,FontSource *fs,int limit_count,const VkExtent2D *extent)
        {
            if(!gc || !fs)
                return(nullptr);

            const uint32_t height=hgl_align_pow2(fs->GetCharHeight()+2,4);

            if(limit_count<=0)
            {
                VkExtent2D ext{1024,1024};
                if(extent)
                    ext=*extent;

                limit_count=(ext.width/height)*(ext.height/height);
                if(limit_count<=0)
                    limit_count=1024;
            }

            auto tm=gc->GetTextureManager();
            if(!tm)
                return(nullptr);

            TileData *td=tm->CreateTileData(UPF_R8,height,height,limit_count);
            if(!td)
                return(nullptr);

            return(new TileFont(td,fs));
        }
    }

    TextRender *TextRender::CreateWithGraphicsContext(GraphicsContext *gc,RenderTargetFormat *rp,FontSource *fs,int limit,const VkExtent2D *extent)
    {
        if(!gc || !fs || !rp)
            return(nullptr);

        TileFont *tile_font=CreateTileFont(gc,fs,limit,extent);
        if(!tile_font)
            return(nullptr);

        TextRender *text_render=new TextRender(gc,tile_font);

        if(!text_render)
        {
            delete tile_font;
            return(nullptr);
        }

        auto sampler_mgr = gc->GetSamplerManager();
        if(!sampler_mgr)
        {
            delete tile_font;
            delete text_render;
            return(nullptr);
        }

        if(!text_render->Init(rp,sampler_mgr->CreateSampler().lock().get()))
        {
            delete tile_font;
            delete text_render;
            return(nullptr);
        }

        return text_render;
    }

    bool TextRender::InitMaterial(RenderTargetFormat *rp)
    {
        mtl::Text2DMaterialCreateConfig mtl_cfg;

        mtl_fs=mtl_manager->AcquireMaterialInternal(mtl::MaterialPreset::Text2D,&mtl_cfg);
        if(!mtl_fs)return(false);

        //文本渲染Position坐标全部是使用整数，这里强制要求Position输入流使用RG16I格式
        {
            VILConfig vil_config;

            vil_config.Add(VAN::Position,VF_V2I16);

            auto *domain = mtl_manager->GetOrCreateDefaultDomain(mtl_fs);
            const VIL *vil = mtl_fs->CreateVIL(&vil_config);

            if (!domain || !vil || !material_registry)
                return(false);

            MaterialBindingInit init;
            init.material = mtl_fs;
            init.domain_handle = mtl_manager->GetIDDManager()->GetHandle(domain);
            init.vil = vil;
            init.preset = GraphicsPipelinePreset::Solid2D;
            init.material_preset = mtl::MaterialPreset::Text2D;
            init.instance_data = &fixed_style;
            init.instance_data_size = sizeof(fixed_style);

            fixed_style_handle = material_registry->AllocateHandle(init);
            if (fixed_style_handle == InvalidMaterialInstanceHandle)
                return(false);

            fixed_vil = vil;
        }

        (void)rp;

        if(!mtl_fs->BindTextureSampler(  mtl::SamplerSlot::Text,
                         tile_font->GetTexture(),
                         sampler))
            return(false);

        return(true);
    }

    bool TextRender::Init(RenderTargetFormat *rp,Sampler *text_sampler)
    {
        sampler=text_sampler;

        if(!InitMaterial(rp))
            return(false);

        return(true);
    }

    TextGeometry *TextRender::Begin(const TextGeometryType &tpt,int limit)
    {
        TextGeometry *tr=new TextGeometry(device,fixed_vil,limit);

        text_geometry_set.Add(tr);

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

    TextGeometry *TextRender::CreateGeometry(const TextGeometryType &tpt,const U16StringView &str)
    {
        TextGeometry *tr=Begin(tpt,str.Length());

        if(!tr)
            return(nullptr);

        if(!SimpleLayout(tr,str))
        {
            delete tr;
            return(nullptr);
        }

        return tr;
    }

    bool TextRender::SimpleLayout(TextGeometry *tr,const U16StringView&str)
    {
        if(!tr)
            return(false);

        if(!tl_engine->Begin(tr,str.Length()))
            return(false);

        const bool result=tl_engine->AddString(str,text_draw_style);

        tl_engine->End();

        return(result);
    }

    Primitive *TextRender::CreatePrimitive(TextGeometry *text_geometry)
    {
        if(primitive_manager)
        {
            if (!material_registry || fixed_style_handle == InvalidMaterialInstanceHandle)
                return(nullptr);

            PrimitiveMaterialSlot slot;
            if (!material_registry->BuildSlot(fixed_style_handle, slot))
                return(nullptr);

            return primitive_manager->CreatePrimitive(text_geometry, slot);
        }

        return(nullptr);
    }

    void TextRender::Release(TextGeometry *tr)
    {
        if(!tr)return;

        if(!text_geometry_set.Delete(tr))return;

        const auto& chars_set = tr->GetCharsSets();
        std::vector<u32char> temp_chars(chars_set.begin(), chars_set.end());
        tile_font->Unregistry(temp_chars);

        delete tr;
    }

}//namespace hgl::graph
