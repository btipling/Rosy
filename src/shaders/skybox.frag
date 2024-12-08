#version 460

layout (location = 0) in vec2 tcOut;

layout (location = 0) out vec4 outFragColor;

//layout(set = 1, binding = 0) uniform sampler2D displayTexture;

void main() 
{
	outFragColor = vec4(1.0, 0.0, 0.0, 1.0);
}