#pragma once
#include <hgl/common/VertexAttribDef.h>
#include <cstdint>
#include <hgl/shadergen/ShaderWriter.h>

namespace hgl::graph {

const char* GetVertexAttribMacroName(VertexAttrib va);

void EmitVertexAttribDefine(ShaderWriter& writer, VertexAttrib attrib);

void EmitVertexAttribDefines(ShaderWriter& writer, uint32_t attrib_mask);

} // namespace hgl::graph
