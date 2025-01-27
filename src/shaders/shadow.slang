import data;

[[vk::push_constant]] ShadowConstant ShadowConstants;

[shader("vertex")]
ShadowOutput shadowVertexMain(uint uiVertexId: SV_VertexID)
{
    InputVertex v = *(ShadowConstants.v + uiVertexId);
    RenderData rd = *ShadowConstants.rd;
    SceneData sd = *ShadowConstants.sd;

    float4x4 worldMat = rd.transform;
    float3x3 worldMat3 = (float3x3)worldMat;

    float4x4 proj = sd.shadowProjNear;
//    if (ShadowConstants.pass_number == 1) {
//        proj = sd.shadowproj_middle;
//    } else if (ShadowConstants.pass_number == 2) {
//        proj = sd.shadowproj_far;
//    }

    ShadowOutput output;
    float4 posWorld = float4(v.position, 1.0);
    output.sv_position = mul(mul(proj, worldMat), posWorld);

    return output;
}
