#version 450

#include "ps1.utility"

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec2 inTexcoord;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) 
uniform sampler2D diffuse;

void main() {
    vec3 diffuse = vec3(texture(diffuse, inTexcoord));
	outColor = ps1_posterize(vec4(diffuse * inColor, 1.0));
}
