import data.data;

[[vk::push_constant]] PushConstant PushConstants;

[shader("vertex")]
ShadowOutput shadowVertexMain(uint uiVertexId: SV_VertexID, int ViewIndex: SV_ViewID)
{
    InputVertex v = *(PushConstants.v + uiVertexId);
    RenderData rd = *PushConstants.rd;
    SceneData sd = *PushConstants.sd;

    float4x4 worldMat = rd.transform;
    if (ViewIndex != 0) {
       worldMat = mul(worldMat, float4x4(
        2, 0, 0, 0,
        0, 2, 0, 0,
        0, 0, 2, 0,
        0, 0, 0, 1
       ));
    }
    float3x3 worldMat3 = (float3x3)worldMat;

    ShadowOutput output;
    float4 posWorld = float4(v.position, 1.0);
    output.sv_position = mul(mul(sd.shadowproj, worldMat), posWorld);
    output.view_index = ViewIndex;

    return output;
}

[shader("fragment")]
Fragment shadowFragmentMain() : SV_Target
{
    Fragment output;
    output.color = float4(1.0, 0.0, 0.0, 1.0);
    return output;
}