MaterialInstance GetMI()
{
#if ShaderStage == VertexShader
    return mtl.mi[Assign.y];
#else
    return mtl.mi[Input.MaterialInstanceID];
#endif
}
