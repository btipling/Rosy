import data;

[[vk::push_constant]] BasicPushConstant BasicPushConstants;

[shader("vertex")]
BasicVertexStageOutput vertexMain(uint uiVertexId: SV_VertexID)
{
    SceneData sd = *BasicPushConstants.sd;
    BasicInputVertex v = *(BasicPushConstants.v + uiVertexId);

    float4 posWorld = float4(v.position, 1.0);

    BasicVertexStageOutput output;
    output.basicVertex.normal = v.normal;
    output.basicVertex.color = v.color;
    output.sv_position = mul(sd.viewproj, posWorld);

    return output;
}

[shader("fragment")]
Fragment fragmentMain(BasicVertex basicVertex: BasicVertex) : SV_Target
{
    Fragment output;
    output.color = float4(basicVertex.normal * 0.5 + 0.5, 1.0);
    return output;
}
