import data;

[[vk::push_constant]] ShadowConstant ShadowConstants;

[shader("vertex")]
ShadowOutput shadowVertexMain(uint uiVertexId: SV_VertexID)
{
    SceneData sd = *ShadowConstants.sd;
    BasicInputVertex v = *(ShadowConstants.v + uiVertexId);
    BasicGraphicsData gd = *ShadowConstants.gd;
    float4x4 worldMat = gd.transform;

    float4 posWorld = float4(v.position, 1.0);

    ShadowOutput output;

    output.sv_position = mul(mul(sd.shadowProjNear, worldMat), posWorld);

    return output;
}
