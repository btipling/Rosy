#version 450 core
layout(location = 0) out vec4 fColor;
layout(set=0, binding=0) uniform sampler2D sTexture;
layout(location = 0) in struct { vec4 Color; vec2 UV; } In;

float srgbToLinear(float srgb) {
    if (srgb <= 0.04045) {
        return srgb / 12.92;
    } else {
        return pow((srgb + 0.055) / 1.055, 2.4);
    }
}

// Convert sRGB color vector to linear
vec3 srgbToLinear(vec3 srgb) {
    return vec3(
        srgbToLinear(srgb.r),
        srgbToLinear(srgb.g),
        srgbToLinear(srgb.b)
    );
}

// Convert sRGB color vector with alpha to linear
vec4 srgbToLinear(vec4 srgb) {
    return vec4(srgbToLinear(srgb.rgb), srgb.a);
}

void main()
{
    fColor = srgbToLinear(In.Color * texture(sTexture, In.UV.st));
}