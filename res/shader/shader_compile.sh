glslangValidator -V -o FlatColor.vert.spv FlatColor.vert
glslangValidator -V -o FlatColor3D.vert.spv FlatColor3D.vert
glslangValidator -V -o OnlyPosition.vert.spv OnlyPosition.vert
glslangValidator -V -o FlatColor.frag.spv FlatColor.frag

glslangValidator -V -o FlatTexture.vert.spv FlatTexture.vert
glslangValidator -V -o FlatTexture.frag.spv FlatTexture.frag

glslangValidator -V -o PositionColor3D.vert.spv PositionColor3D.vert
glslangValidator -V -o OnlyPosition3D.vert.spv OnlyPosition3D.vert

glslangValidator -V -o c_gbuffer.vert.spv c_gbuffer.vert
glslangValidator -V -o c_gbuffer.frag.spv c_gbuffer.frag

glslangValidator -V -o Atomsphere.vert.spv Atomsphere.vert
glslangValidator -V -o Atomsphere.frag.spv Atomsphere.frag

glslangValidator -V -o gbuffer_opaque.vert.spv gbuffer_opaque.vert
glslangValidator -V -o gbuffer_opaque.frag.spv gbuffer_opaque.frag
