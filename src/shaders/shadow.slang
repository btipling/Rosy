import data.data;

StructuredBuffer<InputVertex, Std430DataLayout> VertexBuffer;
StructuredBuffer<RenderData, Std430DataLayout> RenderDataBuffer;
[[vk::binding(0, 0)]] ConstantBuffer<SceneData> sceneData;
[[vk::binding(0, 1)]] texture2D imageTexture[];
[[vk::binding(1, 1)]] SamplerState imageSampler[];

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