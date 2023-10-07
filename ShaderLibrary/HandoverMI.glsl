void HandoverMI()
{
#if ShaderStage == VertexShader
    Output.MaterialInstanceID=Assign.y;
#elif ShaderStage == GeometryShader
    Output.MaterialInstanceID=Input[0].MaterialInstanceID;
#else
    Output.MaterialInstanceID=Input.MaterialInstanceID;
#endif
}
