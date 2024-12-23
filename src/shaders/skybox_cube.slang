
struct CoarseVertex
{
    float3 color;
    float3 normal;
    float3 vertex;
    float2 textureCoordinates;
};

struct Fragment
{
    float4 color;
};

[[vk::binding(0, 1)]] TextureCube imageTexture[];
[[vk::binding(1, 1)]] SamplerState imageSampler[];

[shader("fragment")]
Fragment cubeFragmentMain(CoarseVertex coarseVertex : CoarseVertex) : SV_Target
{
    float4 outFragColor = imageTexture[NonUniformResourceIndex(0)].Sample(imageSampler[NonUniformResourceIndex(0)], coarseVertex.vertex);

    Fragment output;
    output.color = outFragColor;
    return output;
}