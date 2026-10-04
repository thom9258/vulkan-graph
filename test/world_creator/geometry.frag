#version 450

#include "ps1.utility"

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexcoord;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) 
uniform sampler2D diffuse;

//   layout (set = 2, binding = 0, std140)
//   uniform Lights
//   {
//       vec3 direction;
//       vec3 color;
//       float intensity;
//   } light;

void main() {
   //const vec3 normal = normalize(inNormal);
   //const vec3 light_direction = normalize(light.direction);
   //const float lambert = dot(normal, light_direction);
   //const float light_factor = max(lambert, 0.0) * max(lambert, 0.0);
   //const vec3 light = light.color * light.intensity;
   const vec3 albedo = vec3(texture(diffuse, inTexcoord));
   //const vec3 diffuse = albedo * light * light_factor;
   //const vec3 ambient_floor = albedo * vec3(0.04, 0.04, 0.05); 

    outColor = vec4(albedo, 1.0);
}
