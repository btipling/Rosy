import data.data;

[[vk::binding(0, 0)]] texture2D imageTexture[];
[[vk::binding(1, 0)]] SamplerState imageSampler[];

[[vk::push_constant]] PushConstant PushConstants;

[shader("vertex")]
VertexStageOutput vertexMain(uint uiVertexId: SV_VertexID)
{
    InputVertex v = *(PushConstants.v + uiVertexId);
    RenderData rd = *PushConstants.rd;
    SceneData sd = *PushConstants.sd;

    float4x4 worldMat = rd.transform;

    VertexStageOutput output;

    output.coarseVertex.vertex = v.position;
    output.coarseVertex.color = v.color.xyz;
    output.coarseVertex.normal = mul(rd.normalTransform, v.normal);
    output.coarseVertex.textureCoordinates = float2(v.textureCoordinates_x, v.textureCoordinates_y);

    float4 posWorld = float4(v.position, 1.0);

    float4x4 ndc_to_tc = {
        0.5, 0, 0, 0.5,
        0, 0.5, 0, 0.5,
        0, 0, 1, 0,
        0, 0, 0, 1
    };

    output.csm.near = mul(
        mul(
            mul(ndc_to_tc, sd.shadowproj_near), 
            worldMat),
        posWorld
    );
    output.csm.middle = mul(
        mul(
            mul(ndc_to_tc,sd.shadowproj_middle), 
            worldMat
        ), 
        posWorld
    );
    output.csm.far = mul(
        mul(
            mul(ndc_to_tc,sd.shadowproj_far), 
            worldMat
        ), 
        posWorld
    );

    output.sv_position = mul(mul(sd.viewproj, worldMat), posWorld);

    return output;
}

[shader("fragment")]
Fragment fragmentMain(CoarseVertex coarseVertex : CoarseVertex, CSM csm : CSM) : SV_Target
{
    SceneData sd = *PushConstants.sd;
    float3 sunlight = float3(sd.sunlight[2][0], sd.sunlight[2][1], sd.sunlight[2][2]);
    float3 color = coarseVertex.color;
    float3 V = normalize(sd.cameraPosition.xyz - coarseVertex.vertex);
    float3 L = -sunlight;
    float3 N = normalize(coarseVertex.normal);
    float3 H = normalize(L + V);

    float cosTheta = dot(L, N);
    float cosPhi = dot(H, N);

    float3 ambientLight = sd.ambientColor.xyz;
    float3 sunLight = sd.sunlightColor.xyz * max(cosTheta, 0.0);

    MaterialData md = *PushConstants.md;
    uint cti = md.color_texture_index;
    uint csi = md.color_sampler_index;

    texture2D matImageTexture = imageTexture[NonUniformResourceIndex(cti)];
    SamplerState matSampler = imageSampler[NonUniformResourceIndex(csi)];
    float4 imageTextureColor = matImageTexture.Sample(matSampler, coarseVertex.textureCoordinates);

    uint csm_sampler_i = sd.csm_index_sampler;
    uint csm_near_i = sd.csm_index_near;
    uint csm_middle_i = sd.csm_index_middle;
    uint csm_far_i = sd.csm_index_far;

    SamplerState shadowSampler = imageSampler[NonUniformResourceIndex(csm_sampler_i)];
    texture2D shadowNearImage = imageTexture[NonUniformResourceIndex(csm_near_i)];
    float4 nearColor = shadowNearImage.Sample(shadowSampler, float2(csm.near.x, csm.near.y));
    bool InNearShadow = nearColor.z < csm.near.z;
    texture2D shadowMiddleImage = imageTexture[NonUniformResourceIndex(csm_near_i)];
    float4 middleColor = shadowNearImage.Sample(shadowSampler, float2(csm.near.x, csm.near.y));
    bool InMiddleShadow = middleColor.z < csm.middle.z;
    texture2D shadowFarImage = imageTexture[NonUniformResourceIndex(csm_near_i)];
    float4 farColor = shadowNearImage.Sample(shadowSampler, float2(csm.near.x, csm.near.y));
    bool InFarShadow = farColor.z < csm.far.z;
    if (InNearShadow || InMiddleShadow || InFarShadow) {
       // sunLight = float3(0);
    }

    float4 outFragColor = imageTextureColor * float4(ambientLight + sunLight, 1.0);

    Fragment output;
    output.color = outFragColor;
    // output.color = nearColor;
    return output;
}