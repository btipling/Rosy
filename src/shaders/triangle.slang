
static float4 vertices[3] = {
    { -0.5, 0.5, 0.5, 1.0 },
    { 0.0, -0.5, 0.5, 1.0 },
    { 0.5, 0.5, 0.5, 1.0 },
};


[shader("vertex")]
float4 vertexMain(uint uiVertexId: SV_VertexID) : SV_Position
{
    return vertices[uiVertexId];
}

[shader("fragment")]
float4 fragmentMain() : SV_Target
{
    return float4(1.0, 0.0, 1.0, 1.0);
}