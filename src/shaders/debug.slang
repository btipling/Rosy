import data;

[[vk::push_constant]] DebugConstant DebugConstants;

[shader("vertex")]
DebugVertexStageOutput debugVertMain(uint uiVertexId: SV_VertexID)
{
    DebugInputVertex v = *(DebugConstants.v + uiVertexId);
    float4x4 worldMat = DebugConstants.transform;

    DebugVertexStageOutput output;
    output.debugVertex.color = DebugConstants.color.xyz;
    output.sv_position = mul(worldMat, v.position);

    return output;
}

[shader("fragment")]
Fragment debugFragmentMain(DebugVertex debugVertex: DebugVertex) : SV_Target
{
    Fragment output;
    output.color = float4(debugVertex.color, 1.0);
    return output;
}