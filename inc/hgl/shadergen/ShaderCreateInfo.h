#pragma once

#include <hgl/common/VertexAttribDef.h>
#include <hgl/common/ShaderStageDef.h>
#include <hgl/common/InterpolationDef.h>
#include<hgl/log/Log.h>
#include<string>

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

    ShaderStageIO *sdi;                      ///<着色器描述符信息(owned)
    MaterialDescriptorDB *descriptor_db;

    std::string final_shader;

    SPVData *spv_data;

protected:

    bool CompileToSPV();

public:

    ShaderStageIO *GetShaderDescriptorInfo(){return sdi;}
    const ShaderStageIO *GetShaderDescriptorInfo()const{return sdi;}
    const ShaderStage GetShaderStage()const{return shader_stage;}

public:

    ShaderCreateInfo(ShaderStageIO *sdi,MaterialDescriptorDB *m);
    virtual ~ShaderCreateInfo();

    const std::string &GetFinalGLSL()const{return final_shader;}

    void SetFinalGLSL(const std::string &glsl){final_shader=glsl;}
    void SetFinalGLSL(const char *glsl){final_shader=glsl?glsl:"";}

    bool CompileFinalGLSLToSPV();                ///< 直接编译 final_shader 到 SPV

    const uint32 *GetSPVData()const;
    const size_t GetSPVSize()const;
};//class ShaderCreateInfo
}}//namespace hgl::graph
