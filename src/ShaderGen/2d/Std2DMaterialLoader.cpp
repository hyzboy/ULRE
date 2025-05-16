#include"Std2DMaterial.h"
#include"MaterialFileData.h"
#include<hgl/shadergen/MaterialCreateInfo.h>

STD_MTL_NAMESPACE_BEGIN

const char *GetUBOCodes(const AccumMemoryManager::Block *block);

namespace
{
    class Std2DMaterialLoader:public Std2DMaterial
    {
    protected:

        material_file::MaterialFileData *mfd;

    public:

        Std2DMaterialLoader(material_file::MaterialFileData *data,const Material2DCreateConfig *c):Std2DMaterial(c)
        {
            mfd=data;
        }

        ~Std2DMaterialLoader()
        {
            delete mfd;
        }

        bool BeginCustomShader() override
        {
            if(!Std2DMaterial::BeginCustomShader())
                return(false);

            if(mfd->mi_data.data_bytes>0)
            {
                mci->SetMaterialInstance(  mfd->mi_data.code,
                                           mfd->mi_data.data_bytes,
                                           mfd->mi_data.shader_stage_flag_bits);
            }

            for(const auto ubo:mfd->ubo_list)
            {
                const char *ubo_codes=GetUBOCodes(ubo.block);

                mci->AddStruct(ubo.struct_name,ubo_codes);

                mci->AddUBO(ubo.shader_stage_flag_bits,
                            ubo.set,
                            ubo.struct_name,
                            ubo.name);
            }

            return true;
        }

        template<VkShaderStageFlagBits ss,typename SD,typename SCI>
        SD *CommonProc(SCI *sc)
        {
            SD *sd=(SD *)(mfd->shader_data_map[ss]);

            if(!sd)
                return(nullptr);

            sc->AddOutput(sd->output);

            for(auto &s:sd->sampler)
                mci->AddSampler(ss,DescriptorSetType::PerMaterial,s.type,s.name);

            sc->SetMain(sd->code,sd->code_length);     //这里会产生复制这个string，但我不希望产生这个。未来想办法改进

            return sd;
        }

        bool CustomVertexShader(ShaderCreateInfoVertex *vsc) override
        {
            vsc->AddInput(mfd->via_list);

            if(!Std2DMaterial::CustomVertexShader(vsc))
                return(false);

            return CommonProc<VK_SHADER_STAGE_VERTEX_BIT,material_file::VertexShaderData,ShaderCreateInfoVertex>(vsc);
        }

        bool CustomGeometryShader(ShaderCreateInfoGeometry *gsc) override
        {
            material_file::GeometryShaderData *sd=CommonProc<VK_SHADER_STAGE_GEOMETRY_BIT,material_file::GeometryShaderData,ShaderCreateInfoGeometry>(gsc);

            if(!sd)
                return(false);

            gsc->SetGeom(sd->input_prim,sd->output_prim,sd->max_vertices);

            return true;
        }

        bool CustomFragmentShader(ShaderCreateInfoFragment *fsc) override
        {
            return CommonProc<VK_SHADER_STAGE_FRAGMENT_BIT,material_file::FragmentShaderData,ShaderCreateInfoFragment>(fsc);
        }
    };//class Std2DMaterialLoader:public Std2DMaterial
}//namespace

material_file::MaterialFileData *LoadMaterialDataFromFile(const AnsiString &mtl_filename);

MaterialCreateInfo *LoadMaterialFromFile(const GPUDeviceAttribute *dev_attr,const AnsiString &name,Material2DCreateConfig *cfg)
{
    if(name.IsEmpty()||!cfg)
        return(nullptr);

    material_file::MaterialFileData *mfd=LoadMaterialDataFromFile(name);

    if(!mfd)
        return nullptr;

    if(mfd->mi_data.data_bytes>0)
        cfg->material_instance=true;

    cfg->shader_stage_flag_bit=mfd->shader_stage_flag_bit;

    Std2DMaterialLoader mtl(mfd,cfg);

    return mtl.Create(dev_attr);
}
STD_MTL_NAMESPACE_END
