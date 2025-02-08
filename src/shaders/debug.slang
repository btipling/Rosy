import data;

[[vk::push_constant]] DebugConstant DebugConstants;

[shader("vertex")]
DebugVertexStageOutput debugVertMain(uint uiVertexId: SV_VertexID)
{
    SceneData sd = *DebugConstants.sd;
    DebugInputVertex v = *(DebugConstants.v + uiVertexId);
    float4x4 worldMat = DebugConstants.transform;

    DebugVertexStageOutput output;
    output.debugVertex.color = DebugConstants.color.xyz;
    if (DebugConstants.flags == 1) {
        float aspectRatio = sd.drawExtent.x / sd.drawExtent.y;
        output.sv_position = mul(worldMat, (v.position * float4(1.f, aspectRatio, 1.f, 1.f)));
    } else {
        output.sv_position = mul(mul(sd.viewproj, worldMat), v.position);
    }

    return output;
}

[shader("fragment")]
Fragment debugFragmentMain(DebugVertex debugVertex: DebugVertex) : SV_Target
{
    Fragment output;
    output.color = float4(debugVertex.color, 1.0);
    return output;
}