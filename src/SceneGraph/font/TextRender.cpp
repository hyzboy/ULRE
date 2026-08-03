#include<hgl/graph/font/TextRender.h>
#include<hgl/graph/font/TextGeometry.h>
#include<hgl/graph/font/TileFont.h>
#include<hgl/graph/font/TextLayout.h>
#include<hgl/graph/font/TextLayoutEngine.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKVertexInputConfig.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/mtl/SamplerName.h>
#include<hgl/framework/AppFramework.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/ShaderProgramManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/ResourceDomainManager.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/graph/DescriptorBindingSet.h>
#include<hgl/vk/VKShaderProgram.h>
#include<hgl/vk/VKRenderPass.h>
#include<hgl/vk/VKBuffer.h>
#include<hgl/type/AlignUtil.h>
#include<hgl/vk/VKFormat.h>
#include<hgl/color/Color.h>

namespace hgl::graph
{
    using namespace layout;

    namespace
    {
        uint32_t ResolveMaterialSSBOStride(const ShaderProgram *material)
        {
            if (!material)
                return 0;

            for (const auto &req : material->GetMaterialResourceLayout().requirements)
            {
                if (req.semantic != mtl::DescriptorSemantic::MaterialSSBOSlotData)
                    continue;

                return mtl::GetSSBOTypeStructStride(req.ssbo_type);
            }

            return 0;
        }

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
        device              = gc ? gc->GetDevice() : nullptr;
        graphics_context    = gc;
        mtl_manager         = gc ? gc->GetMaterialManager() : nullptr;
        tl_engine           = new layout::TextLayout(tf);

        mtl_fs      =nullptr;
        sampler     =nullptr;
        pipeline    =nullptr;
        tile_font   =tf;

        binding_vil =nullptr;
        binding_set =nullptr;
        mi_ssbo     =nullptr;

        fixed_style.CharColor=GetColor4ub(COLOR::White);

        SetDrawStyle(text_draw_style,&para_style,(float)tile_font->GetFontSource()->GetCharHeight());
    }

    void TextRender::SetFixedStyle(const layout::CharStyle &cs)
    {
        if(!mem_compare(fixed_style,cs))
            return;

        fixed_style=cs;

        // Write updated CharStyle directly to the external SSBO.
        if (mi_ssbo)
        {
            if (auto *gpu = mi_ssbo->GetGPUBuffer())
                gpu->Write(&fixed_style, 0, hgl_min(static_cast<uint32_t>(sizeof(fixed_style)),
                                                    ResolveMaterialSSBOStride(mtl_fs)));
        }
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

        delete binding_set;
        binding_set = nullptr;

        if (mi_ssbo && graphics_context)
        {
            if (auto *buf_mgr = graphics_context->GetBufferManager())
                buf_mgr->Release(mi_ssbo);
        }
        mi_ssbo = nullptr;

        if (binding_vil && mtl_fs)
        {
            mtl_fs->Release(const_cast<VIL *>(binding_vil));
            binding_vil = nullptr;
        }

        SAFE_CLEAR(tl_engine);
        SAFE_CLEAR(tile_font);
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

    TextRender *TextRender::CreateWithGraphicsContext(GraphicsContext *gc,RenderPass *rp,FontSource *fs,int limit,const VkExtent2D *extent)
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

        if(!text_render->Init(rp,sampler_mgr->CreateSampler()))
        {
            delete tile_font;
            delete text_render;
            return(nullptr);
        }

        return text_render;
    }

    bool TextRender::InitMaterial(RenderPass *rp)
    {
        mtl::MaterialRecipe recipe{};
        recipe.mtl_def_id = "Text2D";
        {
            mtl::MaterialDefinitionBuildRequest mtl_request{};
            mtl_request.recipe = recipe;
            mtl_request.primitive_type = PrimitiveType::Triangles;
            mtl_fs = mtl_manager->AcquireMaterialProgram(mtl_request);
        }
        if(!mtl_fs)return(false);

        const GeometryVertexFormat text_gvf = CreateTextGeometryVertexFormat();

        // Build VIL from geometry vertex format
        {
            binding_vil = mtl_fs->CreateVIL(text_gvf);
            if (!binding_vil) return false;
        }

        // Create external SSBO for CharStyle data (one slot)
        auto *buffer_manager = graphics_context ? graphics_context->GetBufferManager() : nullptr;
        auto *domain_manager = graphics_context ? graphics_context->GetResourceDomainManager() : nullptr;

        const uint32_t mi_bytes = ResolveMaterialSSBOStride(mtl_fs);
        if (mi_bytes > 0 && buffer_manager && domain_manager)
        {
            mi_ssbo = buffer_manager->CreateSSBO("TextRender:FixedStyle", mi_bytes, nullptr, SharingMode::Exclusive);
            if (!mi_ssbo) return false;

            // Write initial CharStyle data
            if (auto *gpu = mi_ssbo->GetGPUBuffer())
                gpu->Write(&fixed_style, 0, hgl_min(static_cast<uint32_t>(sizeof(fixed_style)), mi_bytes));

            // Register in domain manager so RenderDescriptorBindingSystem can resolve it
            for (const auto &req : mtl_fs->GetMaterialResourceLayout().requirements)
            {
                if (req.semantic != mtl::DescriptorSemantic::MaterialSSBOSlotData)
                    continue;
                const mtl::SSBOAddress addr{req.ssbo_type, req.ssbo_id, 0};
                domain_manager->RegisterBuffer(addr, mi_ssbo, 1);
            }
        }

        // Build DescriptorBindingSet
        binding_set = new DescriptorBindingSet(mtl_fs, binding_vil);
        if (!binding_set) return false;
        for (const auto &req : mtl_fs->GetMaterialResourceLayout().requirements)
        {
            if (req.semantic != mtl::DescriptorSemantic::MaterialSSBOSlotData)
                continue;
            binding_set->SetSSBOBinding(req.ssbo_type, req.ssbo_id, 0);
        }

        pipeline = rp->CreatePipeline(mtl_fs, binding_vil, PipelinePreset::Solid2D, false, &text_gvf);
        if (!pipeline) return false;

        if (!mtl_fs->BindTextureSampler(DescriptorSetType::Material,
                                        mtl::SamplerName::Text,
                                        tile_font->GetTexture(),
                                        sampler))
            return false;

        return true;
    }

    bool TextRender::Init(RenderPass *rp,Sampler *text_sampler)
    {
        sampler=text_sampler;

        if(!InitMaterial(rp))
            return(false);

        return(true);
    }

    TextGeometry *TextRender::Begin(const TextGeometryType &tpt,int limit)
    {
        TextGeometry *tr=new TextGeometry(device,CreateTextGeometryVertexFormat(),limit);

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
