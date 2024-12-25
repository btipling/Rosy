import data.data;

[[vk::binding(0, 0)]] ConstantBuffer<SceneData> sceneData;

[[vk::push_constant]] DebugConstant DebugConstants;

[shader("vertex")]
DebugVertexStageOutput debugVertMain(uint uiVertexId: SV_VertexID)
{
    float4 v = DebugConstants.p1;
    if (uiVertexId != 0) {
        v = DebugConstants.p2;
    }
    DebugVertexStageOutput output;
    output.debugVertex.color = DebugConstants.color.xyz;
    output.sv_position = mul(DebugConstants.worldMatrix, v);

    return output;
}

[shader("fragment")]
Fragment debugFragmentMain(DebugVertex debugVertex: DebugVertex) : SV_Target
{
    Fragment output;
    output.color = float4(debugVertex.color, 1.0);
    return output;
}