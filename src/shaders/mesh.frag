#version 460
#extension GL_EXT_nonuniform_qualifier : require

layout (location = 0) in vec3 fragmentColor;
layout (location = 1) in vec3 fragmentNormal;
layout (location = 2) in vec3 fragmentVertex;
layout (location = 3) in vec2 textureCoordinates;

layout (location = 0) out vec4 outFragColor;

layout(set = 0, binding = 0) uniform  SceneData{
	mat4 view;
	mat4 proj;
	mat4 viewproj;
	mat4 shadowproj;
	vec4 cameraPosition;
	vec4 ambientColor;
	vec4 sunlightDirection;
	vec4 sunlightColor;
} sceneData;

layout(set = 1, binding = 0) uniform texture2D imageTexture[];
layout(set = 1, binding = 1) uniform sampler imageSampler[];

void main() 
{
	vec3 V = normalize(sceneData.cameraPosition.xyz - fragmentVertex); 
	vec3 L = sceneData.sunlightDirection.xyz;
	vec3 N = normalize(fragmentNormal);
	vec3 H = normalize(L + V);

	float cosTheta = dot(L, N);
	float cosPhi = dot(H, N);

    vec3 ambientLight = sceneData.ambientColor.xyz;
    vec3 sunLight = sceneData.sunlightColor.xyz * max(cosTheta, 0.0);
	outFragColor = texture(nonuniformEXT(sampler2D(imageTexture[0], imageSampler[0])), textureCoordinates) * vec4(ambientLight + sunLight, 1.0);
}