import data.data;

[[vk::push_constant]] PushConstant PushConstants;

[shader("vertex")]
ShadowOutput shadowVertexMain(uint uiVertexId: SV_VertexID)
{
    InputVertex v = *(PushConstants.v + uiVertexId);
    RenderData rd = *PushConstants.rd;
    SceneData sd = *PushConstants.sd;

    float4x4 worldMat = rd.transform;
    float3x3 worldMat3 = (float3x3)worldMat;

    ShadowOutput output;
    float4 posWorld = float4(v.position, 1.0);
    output.sv_position = mul(mul(sd.shadowproj_near, worldMat), posWorld);

    return output;
}