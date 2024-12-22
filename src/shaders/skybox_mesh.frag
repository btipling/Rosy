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

layout(set = 1, binding = 0) uniform sampler2D displayTexture[];

void main() 
{
	outFragColor = texture(displayTexture[nonuniformEXT(0)], textureCoordinates);
}