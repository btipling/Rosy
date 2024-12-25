import data.data;

StructuredBuffer<InputVertex, ScalarDataLayout> VertexBuffer;
StructuredBuffer<RenderData, ScalarDataLayout> RenderDataBuffer;
[[vk::binding(0, 0)]] ConstantBuffer<SceneData> sceneData;

[[vk::push_constant]] PushConstant PushConstants;

[shader("vertex")]
ShadowOutput shadowVertexMain(uint uiVertexId: SV_VertexID)
{
    InputVertex v = *(PushConstants.v + uiVertexId);
    RenderData rd = *PushConstants.rd;

    float4x4 worldMat = rd.transform;
    float3x3 worldMat3 = (float3x3)worldMat;

    ShadowOutput output;
    float4 posWorld = float4(v.position, 1.0);
    output.sv_position = mul(mul(sceneData.viewproj, worldMat), posWorld);

    return output;
}

[shader("fragment")]
void shadowFragmentMain() : SV_Target
{
}