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

    if (DebugConstants.flags == 4) { // transform is actually vertices
        output.sv_position = mul(sd.viewproj, float4(worldMat[0][uiVertexId], worldMat[1][uiVertexId], worldMat[2][uiVertexId], worldMat[3][uiVertexId]));
        return output;
    }

    if (DebugConstants.flags == 1) { // screen space
        float aspectRatio = sd.drawExtent.x / sd.drawExtent.y;
        float4 position = mul(worldMat, (v.position * float4(1.f, aspectRatio, 1.f, 1.f)));
        position.z = 1.f;
        position.w = 1.f;
        output.sv_position = position;
    } else if (DebugConstants.flags == 2) { // view space
        float4 position = mul(mul(sd.proj, worldMat), v.position);
        position.z = 1.f;
        position.w = 1.f;
        output.sv_position = position;
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