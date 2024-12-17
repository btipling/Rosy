#version 460

layout (location = 0) in vec3 fragmentColor;

layout (location = 0) out vec4 outFragColor;

layout(set = 1, binding = 0) uniform samplerCube displayTexture;

void main() 
{
	outFragColor = vec4(fragmentColor, 1.0);
}