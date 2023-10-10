#include"Std2DMaterial.h"
#include"MaterialFileData.h"
#include<hgl/shadergen/MaterialCreateInfo.h>

STD_MTL_NAMESPACE_BEGIN

namespace
{
    class Std2DMaterialLoader:public Std2DMaterial
    {
    protected:

        material_file::MaterialFileData *mfd;

    public:

        Std2DMaterialLoader(material_file::MaterialFileData *data,const Material2DCreateConfig *cfg):Std2DMaterial(cfg)
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

            if(mfd->mi.mi_bytes>0)
            {
                mci->SetMaterialInstance(  mfd->mi.code,
                                           mfd->mi.mi_bytes,
                                           mfd->mi.shader_stage_flag_bits);
                return(false);
            }

            return true;
        }

        bool CustomVertexShader(ShaderCreateInfoVertex *vsc) override
        {
            if(!Std2DMaterial::CustomVertexShader(vsc))
                return(false);

            if(mfd->vi.GetCount()>0)
            {
                for(auto &ua:mfd->vi)
                    vsc->AddInput(ua.vat,ua.name);
            }

            material_file::ShaderData *sd=mfd->shader[VK_SHADER_STAGE_VERTEX_BIT];

            if(!sd)
                return(false);

            for(auto &ua:sd->output)
                vsc->AddOutput(ua.vat,ua.name);

            vsc->SetMain(sd->code,sd->code_length);     //这里会产生复制这个string，但我不希望产生这个。未来想办法改进

            return true;
        }

        bool CustomGeometryShader(ShaderCreateInfoGeometry *gsc) override
        {
            if(!Std2DMaterial::CustomGeometryShader(gsc))
                return(false);
            
            material_file::GeometryShaderData *sd=(material_file::GeometryShaderData *)(mfd->shader[VK_SHADER_STAGE_GEOMETRY_BIT]);

            if(!sd)
                return(false);

            gsc->SetGeom(sd->input_prim,sd->output_prim,sd->max_vertices);

            for(auto &ua:sd->output)
                gsc->AddOutput(ua.vat,ua.name);

            gsc->SetMain(sd->code,sd->code_length);     //这里会产生复制这个string，但我不希望产生这个。未来想办法改进

            return true;
        }

        bool CustomFragmentShader(ShaderCreateInfoFragment *fsc) override
        {
            if(!Std2DMaterial::CustomFragmentShader(fsc))
                return(false);

            material_file::ShaderData *sd=mfd->shader[VK_SHADER_STAGE_FRAGMENT_BIT];

            if(!sd)
                return(false);

            for(auto &ua:sd->output)
                fsc->AddOutput(ua.vat,ua.name);

            fsc->SetMain(sd->code,sd->code_length);     //这里会产生复制这个string，但我不希望产生这个。未来想办法改进

            return true;
        }

        bool EndCustomShader() override{return true;}
    };//class Std2DMaterialLoader:public Std2DMaterial
}//namespace

material_file::MaterialFileData *LoadMaterialDataFromFile(const AnsiString &mtl_filename);

MaterialCreateInfo *LoadMaterialFromFile(const AnsiString &name,const Material2DCreateConfig *cfg)
{
    material_file::MaterialFileData *mfd=LoadMaterialDataFromFile(name);

    if(!mfd)
        return nullptr;

    Std2DMaterialLoader mtl(mfd,cfg);

    return mtl.Create();
}
STD_MTL_NAMESPACE_END
