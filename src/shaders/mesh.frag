#version 460

layout (location = 0) in vec3 fragmentColor;
layout (location = 1) in vec3 fragmentNormal;
layout (location = 2) in vec2 textureCoordinates;

layout (location = 0) out vec4 outFragColor;

layout(set = 1, binding = 0) uniform sampler2D displayTexture;

void main() 
{
	outFragColor = texture(displayTexture, textureCoordinates);
}