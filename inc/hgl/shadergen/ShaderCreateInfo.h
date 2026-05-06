#pragma once

#include <hgl/common/VertexAttribDef.h>
#include <hgl/common/ShaderStageDef.h>
#include <hgl/common/InterpolationDef.h>
#include<hgl/log/Log.h>
#include<string>
#include<memory>

namespace hgl{namespace graph
{
struct SPVData;

class MaterialDescriptorDB;
class ShaderStageIO;

struct UBODescriptor;
struct SSBODescriptor;
struct TextureDescriptor;
struct TextureSamplerDescriptor;

class ShaderCreateInfo
{
    OBJECT_LOGGER

protected:

    ShaderStage shader_stage;                      ///<着色器阶段

    struct SPVDataDeleter
    {
        void operator()(SPVData *ptr) const noexcept;
    };

    std::unique_ptr<ShaderStageIO> stage_io;      ///<着色器描述符信息(owned)
    MaterialDescriptorDB *descriptor_db;

    std::string final_shader;

    std::unique_ptr<SPVData,SPVDataDeleter> spv_data;

protected:

    bool CompileToSPV();

public:

    ShaderStageIO *GetShaderStageIO(){return stage_io.get();}
    const ShaderStageIO *GetShaderStageIO()const{return stage_io.get();}
    const ShaderStage GetShaderStage()const{return shader_stage;}

public:

    ShaderCreateInfo(ShaderStageIO *stage_io,MaterialDescriptorDB *m);
    virtual ~ShaderCreateInfo();

    const std::string &GetFinalGLSL()const{return final_shader;}

    void SetFinalGLSL(const std::string &glsl){final_shader=glsl;}
    void SetFinalGLSL(const char *glsl){final_shader=glsl?glsl:"";}

    bool CompileFinalGLSLToSPV();                ///< 直接编译 final_shader 到 SPV

    const uint32 *GetSPVData()const;
    const size_t GetSPVSize()const;
};//class ShaderCreateInfo
}}//namespace hgl::graph
